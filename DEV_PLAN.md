# rdemonitor 개발 계획

## 1. 프로젝트 개요

**목적**: RetroDeveloperEnvironmentProject의 에뮬레이터(AppleWin/openMSX)로부터 TCP로 전송되는 JSON Lines 형식의 디버그 데이터를 실시간으로 수신하여 ncurses 기반 TUI로 표시하는 모니터링 프로그램

**주요 특징**:
- ncurses 기반 C 언어 프로그램
- 4개 탭 UI (기본정보/IO/CPU/Memory)
- 실시간 데이터 수신 및 표시
- 검색 및 스냅샷 기능

---

## 2. 프로젝트 구조

```
RetroDeveloperEnvironmentMonitor/
├── src/
│   ├── main.c              # 진입점, 인자 파싱
│   ├── config.c/h          # 설정 파일 관리
│   ├── network.c/h         # TCP 소켓 통신
│   ├── parser.c/h          # JSON 파싱
│   ├── ui.c/h              # ncurses UI 메인
│   ├── ui_tabs.c/h         # 탭 관리
│   ├── ui_tab_info.c/h     # Tab 1: 기본정보
│   ├── ui_tab_io.c/h       # Tab 2: IO 정보
│   ├── ui_tab_cpu.c/h      # Tab 3: CPU 정보
│   ├── ui_tab_memory.c/h   # Tab 4: Memory (hex viewer)
│   ├── data_store.c/h      # 데이터 저장소 (탭별 버퍼)
│   ├── search.c/h          # 검색 기능
│   ├── snapshot.c/h        # 스냅샷 저장
│   └── logger.c/h          # log_all 기능
├── Makefile
├── rdemonitor.config.sample
└── README.md
```

---

## 3. 모듈별 상세 설계

### 3.1 main.c - 진입점

```c
// 주요 기능
- 커맨드 라인 인자 파싱 (--debug_address, --debug_port, --log_all)
- 설정 파일 로드 (command line 우선)
- 각 모듈 초기화
- 메인 이벤트 루프 실행
- 종료 시 정리

// 인자 형식
rdemonitor --debug_address=localhost --debug_port=65505 --log_all=true
```

### 3.2 config.c/h - 설정 관리

```c
typedef struct {
    char debug_address[256];    // 기본값: "localhost"
    int  debug_port;            // 기본값: 6502
    int  log_all;               // 기본값: false(0)
} Config;

// 함수
int config_load(Config *cfg);           // 설정 파일 로드
int config_parse_args(Config *cfg, int argc, char **argv); // CLI 인자 파싱
void config_create_default(void);       // 기본 설정 파일 생성

// 설정 파일 탐색 순서
1. ./rdemonitor.config
2. ~/.rdemonitor.config

// 설정 파일 형식 (텍스트)
debug_address=localhost
debug_port=6502
log_all=false
```

### 3.3 network.c/h - 네트워크 통신

```c
typedef struct {
    int sockfd;
    char recv_buffer[4096];
    int buffer_len;
    int connected;
} NetworkContext;

// 함수
int network_init(NetworkContext *ctx);
int network_connect(NetworkContext *ctx, const char *address, int port);
int network_read_line(NetworkContext *ctx, char *line, int max_len); // 줄 단위 읽기
void network_close(NetworkContext *ctx);

// 비동기 처리
- select() 또는 poll()로 논블로킹 읽기
- ncurses 입력과 네트워크 수신 동시 처리
```

### 3.4 parser.c/h - JSON 파싱

```c
typedef struct {
    char emu[16];       // "apple" | "msx"
    char cat[16];       // 카테고리
    char sec[32];       // 섹션
    char fld[32];       // 필드
    char val[256];      // 값
    char addr[16];      // 주소 (선택)
    int  idx;           // 인덱스 (선택, -1이면 없음)
    long ts;            // 타임스탬프 (선택)
    int  len;           // 길이 (선택)
} ParsedData;

// 함수
int parser_parse_line(const char *json_line, ParsedData *data);
```

**JSON 파싱 라이브러리**: cJSON (경량, 단일 헤더) 사용 권장

### 3.5 data_store.c/h - 데이터 저장소

```c
// === Tab 1: Info ===
typedef struct {
    char emu_type[16];      // "apple" | "msx"
    char machine_id[64];
    char machine_name[128];
    char machine_type[32];
    char status_mode[32];   // "running", "paused"
    char cpu_type[32];
    char emu_version[64];
    // 기타 mach/sys 정보
} InfoData;

// === Tab 2: IO ===
typedef struct {
    char key[64];           // 고유 키 (sec + fld + addr 조합)
    char sec[32];
    char fld[32];
    char addr[16];
    char val[256];
    int  idx;
    long ts;
} IOEntry;

typedef struct {
    IOEntry entries[2048];  // 2048 line buffer
    int count;
    int head;               // circular buffer
} IOData;

// === Tab 3: CPU ===
typedef struct {
    char key[64];           // fld (register name)
    char sec[32];           // "reg", "flag", "int", "state"
    char fld[32];
    char val[256];
} CPUEntry;

typedef struct {
    CPUEntry entries[128];  // 레지스터는 많지 않음
    int count;
} CPUData;

// === Tab 4: Memory ===
typedef struct {
    uint32_t address;       // 시작 주소
    uint8_t  data[16];      // 16바이트 (한 줄)
    int      data_len;
} MemLine;

typedef struct {
    MemLine *lines;         // 동적 배열 (무제한)
    int count;
    int capacity;
    int scroll_pos;         // 스크롤 위치
} MemoryData;

// 함수
void datastore_init(void);
void datastore_process(const ParsedData *data); // 데이터 분류 및 저장
InfoData* datastore_get_info(void);
IOData* datastore_get_io(void);
CPUData* datastore_get_cpu(void);
MemoryData* datastore_get_memory(void);
```

### 3.6 ui.c/h - UI 메인

```
UI 구조:
┌─────────────────────────────────────────────────────────────┐
│ [MSX] Connected                            CAPS: ON         │ <- 상태 바
├─────────────────────────────────────────────────────────────┤
│ [1:Info] [2:IO] [3:CPU] [4:Memory]                          │ <- 탭 바
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                     (탭 내용 영역)                           │
│                                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
│ Shift+1~4:Tab  f:Search  s:Snapshot  q:Quit                 │ <- 도움말
```

```c
// 함수
int ui_init(void);
void ui_update(void);
void ui_handle_input(int ch);
void ui_resize(void);           // SIGWINCH 처리
void ui_cleanup(void);

// Caps Lock 감지
- Linux: /sys/class/leds/input*/capslock/brightness 또는 X11
- 또는 ioctl(fd, KDGETLED, &state)
```

### 3.7 ui_tabs.c/h - 탭 관리

```c
typedef enum {
    TAB_INFO = 0,
    TAB_IO,
    TAB_CPU,
    TAB_MEMORY,
    TAB_COUNT
} TabType;

typedef struct {
    TabType current_tab;
    char search_term[TAB_COUNT][256];   // 탭별 검색어
} TabState;

// 함수
void tabs_init(void);
void tabs_switch(TabType tab);          // Shift+1,2,3,4
void tabs_draw_bar(WINDOW *win);        // 탭 바 그리기
```

### 3.8 ui_tab_info.c/h - Tab 1: 기본정보

```
표시 내용 (mach, sys 카테고리):
┌─ Basic Information ─────────────────────────┐
│ Emulator    : openMSX 19.1                  │
│ Machine ID  : Panasonic_A1F                 │
│ Machine Name: Panasonic FS-A1F              │
│ Machine Type: MSXturboR                     │
│ CPU Type    : R800                          │
│ Status      : Running                       │
│ Video Mode  : Screen 5                      │
│ Connected   : 2024-12-29 14:30:00           │
└─────────────────────────────────────────────┘
```

```c
void tab_info_draw(WINDOW *win);
void tab_info_search(const char *term);       // 검색 하이라이트
```

### 3.9 ui_tab_io.c/h - Tab 2: IO 정보

```
표시 내용 (io 카테고리):
최신 데이터가 아래, 같은 address는 replace

┌─ I/O Information (2048 lines max) ──────────┐
│ [port  ] read  A0 = FF                      │
│ [port  ] write A1 = 00                      │
│ [switch] 80store C000 = 1                   │
│ [slot  ] type   #6 = Disk II                │
│ ...                                         │
│ [port  ] write A1 = 55    <- 최신 (replace) │
└─────────────────────────────────────────────┘
```

```c
void tab_io_draw(WINDOW *win);
void tab_io_search(const char *term);
```

### 3.10 ui_tab_cpu.c/h - Tab 3: CPU 정보

```
표시 내용 (cpu 카테고리):
같은 register는 replace

┌─ CPU Registers ─────────────────────────────┐
│ PC: C600   SP: FF    A: 20                  │
│ X:  00     Y:  01    P: 30                  │
├─ Flags ─────────────────────────────────────┤
│ N:0  V:0  B:0  D:0  I:0  Z:1  C:0           │
├─ Interrupt ─────────────────────────────────┤
│ Enabled: 1                                  │
├─ State ─────────────────────────────────────┤
│ Type: 65C02   Cycles: 12345678              │
└─────────────────────────────────────────────┘
```

```c
void tab_cpu_draw(WINDOW *win);
void tab_cpu_search(const char *term);
```

### 3.11 ui_tab_memory.c/h - Tab 4: Memory (Hex Viewer)

```
Hex Editor 스타일 표시:
같은 address는 replace, 스크롤 가능

┌─ Memory Dump ───────────────────────────────────────────────┐
│ Address  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F    │
├─────────────────────────────────────────────────────────────┤
│ 0000:    A9 20 85 00 A9 00 85 01 20 00 C6 4C 00 C6 00 00    │
│ 0010:    FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF    │
│ 0020:    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00    │
│ ...                                                         │
│ C600:    A9 20 85 00 A9 00 85 01    <- 검색 결과 반전 표시   │
└─────────────────────────────────────────────────────────────┘
│ ↑↓: Line  PgUp/PgDn: Page  Home/End: Start/End              │
```

```c
void tab_memory_draw(WINDOW *win);
void tab_memory_search(const char *term);
void tab_memory_scroll(int direction);   // UP/DOWN/PGUP/PGDN/HOME/END
```

### 3.12 search.c/h - 검색 기능

```c
typedef struct {
    char term[256];
    int  active;
    int  match_count;
} SearchState;

// 함수
void search_show_dialog(void);              // 'f' 키 입력 시
void search_execute(TabType tab, const char *term);
int  search_is_match(const char *text, const char *term);  // 대소문자 무시
void search_clear(TabType tab);
```

```
UI: 검색 dialog
┌─ Search ────────────────────────┐
│ Enter search term: ________     │
│ [Enter] to search, [ESC] cancel │
└─────────────────────────────────┘
```

### 3.13 snapshot.c/h - 스냅샷 기능

```c
// 's' 키 입력 시 모든 탭 내용을 파일로 저장
// 파일명: emulator_snap_YYYYMMDDHHmmss.tsnap

void snapshot_save(void);
```

```
파일 형식 (텍스트):
=== Snapshot: 2024-12-29 14:30:00 ===

=== TAB 1: Info ===
Emulator: openMSX 19.1
Machine: Panasonic FS-A1F
...

=== TAB 2: IO ===
[port] read A0 = FF
...

=== TAB 3: CPU ===
PC: C600  SP: FF  A: 20
...

=== TAB 4: Memory ===
0000: A9 20 85 00 ...
0010: FF FF FF FF ...
... (스크롤 포함 전체 메모리 덤프)
```

### 3.14 logger.c/h - 로그 기능

```c
// log_all=true 시 모든 수신 데이터 기록
// 파일명: emulator_log_YYYYMMDDHHmmss.log

typedef struct {
    FILE *fp;
    int   enabled;
    char  filename[256];
} Logger;

void logger_init(int enabled);
void logger_write(const char *raw_line);    // 원본 JSON 라인 기록
void logger_close(void);
```

---

## 4. 키 바인딩

| 키 | 기능 |
|---|---|
| `Shift+1` | Tab 1 (Info) 전환 |
| `Shift+2` | Tab 2 (IO) 전환 |
| `Shift+3` | Tab 3 (CPU) 전환 |
| `Shift+4` | Tab 4 (Memory) 전환 |
| `f` | 현재 탭 검색 dialog |
| `s` | 스냅샷 저장 |
| `q` | 프로그램 종료 |
| `↑/↓` | Tab 4에서 라인 스크롤 |
| `PgUp/PgDn` | Tab 4에서 페이지 스크롤 |
| `Home/End` | Tab 4에서 처음/끝으로 이동 |
| `ESC` | 검색 취소, dialog 닫기 |

---

## 5. 데이터 처리 로직

### 5.1 카테고리별 탭 매핑

| cat | 저장 탭 |
|-----|--------|
| `sys` | Tab 1 (연결 정보) |
| `mach` | Tab 1 (머신 정보) |
| `io` | Tab 2 |
| `cpu` | Tab 3 |
| `mem` | Tab 4 |
| `dbg` | 모든 탭에 영향 (breakpoint 등) |

### 5.2 Replace 로직

```c
// Tab 2 (IO): key = sec + ":" + fld + ":" + addr
// 예: "port:read:A0"
// 동일 key면 기존 항목 업데이트, 다르면 새로 추가

// Tab 3 (CPU): key = sec + ":" + fld
// 예: "reg:pc", "flag:z"
// 동일 key면 값만 업데이트

// Tab 4 (Memory): key = address
// address 정렬하여 저장, 동일 주소면 데이터 업데이트
```

---

## 6. 빌드 시스템

### Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lncurses -lcjson

TARGET = rdemonitor
SRCDIR = src
OBJDIR = obj

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

install:
	cp $(TARGET) /usr/local/bin/

.PHONY: clean install
```

---

## 7. 개발 순서 (권장)

### Phase 1: 기본 인프라
1. `config.c/h` - 설정 파일 및 CLI 인자 처리
2. `network.c/h` - TCP 연결 및 데이터 수신
3. `parser.c/h` - JSON 파싱
4. `logger.c/h` - 로그 기능

### Phase 2: 데이터 저장
5. `data_store.c/h` - 탭별 데이터 구조 및 저장 로직

### Phase 3: UI 기본
6. `ui.c/h` - ncurses 초기화, 메인 레이아웃
7. `ui_tabs.c/h` - 탭 전환

### Phase 4: 탭 구현
8. `ui_tab_info.c/h` - Tab 1
9. `ui_tab_io.c/h` - Tab 2
10. `ui_tab_cpu.c/h` - Tab 3
11. `ui_tab_memory.c/h` - Tab 4 (스크롤 포함)

### Phase 5: 부가 기능
12. `search.c/h` - 검색 기능
13. `snapshot.c/h` - 스냅샷 저장

### Phase 6: 통합 및 최적화
14. `main.c` - 전체 통합
15. 이벤트 루프 최적화 (select/poll)
16. 터미널 리사이즈 처리

---

## 8. 외부 의존성

| 라이브러리 | 용도 | 설치 |
|-----------|------|-----|
| ncurses | TUI 렌더링 | `apt install libncurses-dev` |
| cJSON | JSON 파싱 | `apt install libcjson-dev` 또는 소스 포함 |

---

## 9. 설정 파일 예시

```
# rdemonitor.config
debug_address=localhost
debug_port=6502
log_all=false
```

---

## 10. 주요 고려사항

### 10.1 비동기 처리
- ncurses의 `getch()`와 네트워크 수신을 동시에 처리해야 함
- `select()` 또는 `poll()`로 stdin과 socket fd를 모니터링
- `timeout()` 또는 `nodelay()` 사용하여 non-blocking input

### 10.2 메모리 관리
- Tab 4의 메모리 데이터는 무제한이므로 동적 할당 필요
- 연결 해제 시 모든 메모리 정리

### 10.3 에러 처리
- 연결 끊김 시 재연결 시도 또는 대기 상태 표시
- 잘못된 JSON 라인 무시 (에러 로그)

### 10.4 Shift+숫자 키 감지
- ncurses에서 Shift+숫자는 특수 문자로 매핑됨
- `!`, `@`, `#`, `$` 로 감지 (US 키보드 기준)
- 또는 keypad 모드 활성화 후 별도 처리

---

## 11. 참조 문서

- [RetroDeveloperEnvironmentProject_OUTPUT_SPEC_V01.md](./RetroDeveloperEnvironmentProject_OUTPUT_SPEC_V01.md) - 데이터 프로토콜 규격
