// cc k8o4.c -o k8o4 -Wall -Wextra -pedantic -std=c99
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#define VSCODE_CLI_VERSION "1.2.1"
#define TAB_STOP 4
#define QUIT_TIMES 2
#define MAX_UNDO 1000
#define PL_RIGHT_ARROW "\uE0B0"
#define COLOR_BG            "\x1b[48;2;30;30;30m"
#define COLOR_FG            "\x1b[38;2;212;212;212m"
#define COLOR_TITLE_BG      "\x1b[48;2;15;15;15m"
#define COLOR_STATUS_FG     "\x1b[38;2;255;255;255m"
#define COLOR_STATUS_ALT_BG "\x1b[48;2;60;60;60m"
#define COLOR_LINENO        "\x1b[38;2;100;100;100m"
#define COLOR_LINENO_CURRENT "\x1b[38;2;220;220;220m"
#define COLOR_SIDEBAR_BORDER "\x1b[38;2;80;80;80m"
#define COLOR_SELECTION_BG  "\x1b[48;2;40;78;121m" 
#define COLOR_KEYWORD1      "\x1b[38;2;86;156;214m"
#define COLOR_KEYWORD2      "\x1b[38;2;197;134;192m"
#define COLOR_STRING        "\x1b[38;2;206;145;120m"
#define COLOR_COMMENT       "\x1b[38;2;106;153;85m"
#define COLOR_NUMBER        "\x1b[38;2;181;206;168m"
#define COLOR_MATCH         "\x1b[48;2;80;80;0m"
#define COLOR_RESET         "\x1b[0m"
enum editorKey {
  BACKSPACE = 127, ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN,
  DEL_KEY, HOME_KEY, END_KEY, PAGE_UP, PAGE_DOWN,
  CTRL_ARROW_LEFT, CTRL_ARROW_RIGHT, CTRL_ARROW_UP, CTRL_ARROW_DOWN,
  SHIFT_ARROW_LEFT, SHIFT_ARROW_RIGHT, SHIFT_ARROW_UP, SHIFT_ARROW_DOWN,
  SHIFT_HOME_KEY, SHIFT_END_KEY,
  CTRL_SHIFT_ARROW_LEFT, CTRL_SHIFT_ARROW_RIGHT,
  CTRL_DEL_KEY,
};
enum editorHighlight {
  HL_NORMAL = 0, HL_COMMENT, HL_MLCOMMENT, HL_KEYWORD1, HL_KEYWORD2,
  HL_STRING, HL_NUMBER, HL_MATCH
};
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
struct editorSyntax {
  char *filetype; char **filematch; char **keywords;
  char *singleline_comment_start; char *multiline_comment_start;
  char *multiline_comment_end; int flags;
};
typedef struct erow {
  int idx; int size; int rsize; char *chars; char *render;
  unsigned char *hl; int hl_open_comment;
} erow;

// Undo/Redo system
enum undoType {
  UNDO_INSERT_CHAR,
  UNDO_DELETE_CHAR,
  UNDO_INSERT_NEWLINE,
  UNDO_DELETE_NEWLINE,
  UNDO_DELETE_SELECTION,
};

typedef struct undoState {
  enum undoType type;
  int cy, cx;           // Position where change occurred
  int prev_cy, prev_cx; // Cursor position before operation
  char c;               // For single char operations
  char *text;           // For multi-char/line operations
  int text_len;
  // For selection deletions
  int sel_start_cy, sel_start_cx;
  int sel_end_cy, sel_end_cx;
  char **lines;         // Deleted lines content
  int num_lines;
  struct undoState *prev;
  struct undoState *next;
} undoState;

struct editorConfig {
  int cx, cy; int rx; int rowoff; int coloff; int screenrows; int screencols;
  int numrows; erow *row; int dirty; char *filename; char statusmsg[80];
  time_t statusmsg_time; struct editorSyntax *syntax; struct termios orig_termios;
  int sidebar_visible; int editor_width; int selection_active;
  int sel_start_cy, sel_start_cx; int sel_end_cy, sel_end_cx;
  undoState *undo_head;
  undoState *undo_current;
  int undo_count;
  int in_undo;  // Flag to prevent recording undo during undo/redo
};
struct editorConfig E;
char DYNAMIC_COLOR_STATUS_BG[32];
char DYNAMIC_COLOR_STATUS_FG_ARROW[32];
char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "short|", "auto|", "const|", "extern|", "register|", "volatile|",
    NULL};
struct editorSyntax HLDB[] = {
    {"c", C_HL_extensions, C_HL_keywords, "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorMoveCursor(int key);
void editorClearSelection();
void editorStartOrExtendSelection(int key);

// Undo system forward declarations
void undoPush(enum undoType type, int cy, int cx, char c, char *text, int text_len);
void undoFreeState(undoState *state);
void editorUndo();
void editorRedo();

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);
  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[5];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        } else if (seq[2] == ';') {
          if (read(STDIN_FILENO, &seq[3], 1) != 1) return '\x1b';
          if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';
          if (seq[1] == '1') {
            switch(seq[3]) {
              case '2': // Shift
                switch (seq[4]) {
                  case 'H': return SHIFT_HOME_KEY;
                  case 'F': return SHIFT_END_KEY;
                  case 'A': return SHIFT_ARROW_UP; case 'B': return SHIFT_ARROW_DOWN;
                  case 'C': return SHIFT_ARROW_RIGHT; case 'D': return SHIFT_ARROW_LEFT;
                }
                break;
              case '5': // Ctrl
                switch (seq[4]) {
                  case 'A': return CTRL_ARROW_UP; case 'B': return CTRL_ARROW_DOWN;
                  case 'C': return CTRL_ARROW_RIGHT; case 'D': return CTRL_ARROW_LEFT;
                }
                break;
              case '6': // Ctrl + Shift
                switch (seq[4]) {
                  case 'C': return CTRL_SHIFT_ARROW_RIGHT;
                  case 'D': return CTRL_SHIFT_ARROW_LEFT;
                }
                break;
            }
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
        case 'c': return CTRL_ARROW_RIGHT; 
        case 'd': return CTRL_ARROW_LEFT;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}
int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);
  if (E.syntax == NULL) return;
  char **keywords = E.syntax->keywords;
  char *scs = E.syntax->singleline_comment_start;
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;
  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;
  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;
    if (scs_len && !in_string && !in_comment) {
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }
    if (mcs_len && mce_len && !in_string) {
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mce_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          i += mce_len; in_comment = 0; prev_sep = 1;
          continue;
        } else { i++; continue; }
      } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
        memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        i += mcs_len; in_comment = 1; continue;
      }
    }
    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING; i += 2; continue;
        }
        if (c == in_string) in_string = 0;
        i++; prev_sep = 1; continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c; row->hl[i] = HL_STRING; i++; continue;
        }
      }
    }
    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER; i++; prev_sep = 0; continue;
      }
    }
    if (prev_sep) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2) klen--;
        if (!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen; break;
        }
      }
      if (keywords[j] != NULL) { prev_sep = 0; continue; }
    }
    prev_sep = is_separator(c);
    i++;
  }
  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx + 1]);
}
const char *editorSyntaxToAnsiColor(int hl) {
  switch (hl) {
  case HL_COMMENT: case HL_MLCOMMENT: return COLOR_COMMENT;
  case HL_KEYWORD1: return COLOR_KEYWORD1; case HL_KEYWORD2: return COLOR_KEYWORD2;
  case HL_STRING: return COLOR_STRING; case HL_NUMBER: return COLOR_NUMBER;
  case HL_MATCH: return COLOR_MATCH; default: return COLOR_FG;
  }
}
void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) return;
  char *ext = strrchr(E.filename, '.');
  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;
        for (int filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
  }
}
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t') rx += (TAB_STOP - 1) - (rx % TAB_STOP);
    rx++;
  }
  return rx;
}
int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t') cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
    cur_rx++;
    if (cur_rx > rx) return cx;
  }
  return cx;
}
void editorUpdateRow(erow *row) {
  int tabs = 0;
  for (int j = 0; j < row->size; j++) if (row->chars[j] == '\t') tabs++;
  free(row->render);
  row->render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);
  int idx = 0;
  for (int j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
  editorUpdateSyntax(row);
}
void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;
  E.row[at].idx = at; E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0; E.row[at].render = NULL; E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);
  E.numrows++; 
  if (!E.in_undo) E.dirty++;
}
void editorFreeRow(erow *row) {
  free(row->render); free(row->chars); free(row->hl);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++) E.row[j].idx--;
  E.numrows--; 
  if (!E.in_undo) E.dirty++;
}
void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++; row->chars[at] = c;
  editorUpdateRow(row); 
  if (!E.in_undo) E.dirty++;
}
void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len; row->chars[row->size] = '\0';
  editorUpdateRow(row); 
  if (!E.in_undo) E.dirty++;
}
void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--; editorUpdateRow(row); 
  if (!E.in_undo) E.dirty++;
}

// Undo system implementation
void undoFreeState(undoState *state) {
  if (!state) return;
  if (state->text) free(state->text);
  if (state->lines) {
    for (int i = 0; i < state->num_lines; i++) {
      if (state->lines[i]) free(state->lines[i]);
    }
    free(state->lines);
  }
  free(state);
}

void undoClearRedo() {
  // Clear all redo states (everything after current)
  if (!E.undo_current) return;
  undoState *next = E.undo_current->next;
  while (next) {
    undoState *tmp = next->next;
    undoFreeState(next);
    E.undo_count--;
    next = tmp;
  }
  if (E.undo_current) E.undo_current->next = NULL;
}

void undoPush(enum undoType type, int cy, int cx, char c, char *text, int text_len) {
  if (E.in_undo) return;  // Don't record undo during undo/redo operations
  
  undoState *state = malloc(sizeof(undoState));
  state->type = type;
  state->cy = cy;
  state->cx = cx;
  state->prev_cy = E.cy;
  state->prev_cx = E.cx;
  state->c = c;
  state->text = NULL;
  state->text_len = text_len;
  state->lines = NULL;
  state->num_lines = 0;
  state->next = NULL;
  
  if (text && text_len > 0) {
    state->text = malloc(text_len + 1);
    memcpy(state->text, text, text_len);
    state->text[text_len] = '\0';
  }
  
  // For selection operations
  if (type == UNDO_DELETE_SELECTION) {
    state->sel_start_cy = E.sel_start_cy;
    state->sel_start_cx = E.sel_start_cx;
    state->sel_end_cy = E.sel_end_cy;
    state->sel_end_cx = E.sel_end_cx;
  }
  
  // Clear redo history
  undoClearRedo();
  
  // Add to undo stack
  if (E.undo_current) {
    E.undo_current->next = state;
    state->prev = E.undo_current;
  } else {
    state->prev = NULL;
    E.undo_head = state;
  }
  E.undo_current = state;
  E.undo_count++;
  
  // Limit undo stack size
  while (E.undo_count > MAX_UNDO) {
    undoState *old = E.undo_head;
    E.undo_head = old->next;
    if (E.undo_head) E.undo_head->prev = NULL;
    undoFreeState(old);
    E.undo_count--;
  }
}

void undoPushSelectionDeletion() {
  if (E.in_undo || !E.selection_active) return;
  
  // Save the deleted content
  int start_cy = E.sel_start_cy;
  int start_cx = E.sel_start_cx;
  int end_cy = E.sel_end_cy;
  int end_cx = E.sel_end_cx;
  
  undoState *state = malloc(sizeof(undoState));
  state->type = UNDO_DELETE_SELECTION;
  state->cy = start_cy;
  state->cx = start_cx;
  state->prev_cy = E.cy;
  state->prev_cx = E.cx;
  state->sel_start_cy = start_cy;
  state->sel_start_cx = start_cx;
  state->sel_end_cy = end_cy;
  state->sel_end_cx = end_cx;
  state->text = NULL;
  state->text_len = 0;
  state->c = 0;
  state->next = NULL;
  state->prev = NULL;
  
  // Save deleted lines
  if (start_cy == end_cy) {
    state->num_lines = 1;
    state->lines = malloc(sizeof(char*));
    int len = end_cx - start_cx;
    state->lines[0] = malloc(len + 1);
    memcpy(state->lines[0], &E.row[start_cy].chars[start_cx], len);
    state->lines[0][len] = '\0';
  } else {
    state->num_lines = end_cy - start_cy + 1;
    state->lines = malloc(sizeof(char*) * state->num_lines);
    
    // First line
    erow *row = &E.row[start_cy];
    int len = row->size - start_cx;
    state->lines[0] = malloc(len + 1);
    memcpy(state->lines[0], &row->chars[start_cx], len);
    state->lines[0][len] = '\0';
    
    // Middle lines
    for (int i = 1; i < state->num_lines - 1; i++) {
      row = &E.row[start_cy + i];
      state->lines[i] = malloc(row->size + 1);
      memcpy(state->lines[i], row->chars, row->size);
      state->lines[i][row->size] = '\0';
    }
    
    // Last line
    row = &E.row[end_cy];
    state->lines[state->num_lines - 1] = malloc(end_cx + 1);
    memcpy(state->lines[state->num_lines - 1], row->chars, end_cx);
    state->lines[state->num_lines - 1][end_cx] = '\0';
  }
  
  undoClearRedo();
  
  if (E.undo_current) {
    E.undo_current->next = state;
    state->prev = E.undo_current;
  } else {
    E.undo_head = state;
  }
  E.undo_current = state;
  E.undo_count++;
  
  while (E.undo_count > MAX_UNDO) {
    undoState *old = E.undo_head;
    E.undo_head = old->next;
    if (E.undo_head) E.undo_head->prev = NULL;
    undoFreeState(old);
    E.undo_count--;
  }
}

void editorUndo() {
  if (!E.undo_current) {
    editorSetStatusMessage("Nothing to undo");
    return;
  }
  
  E.in_undo = 1;
  undoState *state = E.undo_current;
  
  switch (state->type) {
    case UNDO_INSERT_CHAR:
      // Undo character insertion by deleting it
      E.cy = state->cy;
      E.cx = state->cx + 1;
      if (E.cy < E.numrows) {
        editorRowDelChar(&E.row[E.cy], state->cx);
      }
      E.cx = state->cx;
      break;
      
    case UNDO_DELETE_CHAR:
      // Undo character deletion by inserting it back
      E.cy = state->cy;
      E.cx = state->cx;
      if (E.cy < E.numrows) {
        editorRowInsertChar(&E.row[E.cy], state->cx, state->c);
      }
      break;
      
    case UNDO_INSERT_NEWLINE:
      // Undo newline insertion
      E.cy = state->cy + 1;
      E.cx = 0;
      if (E.cy < E.numrows && state->cy >= 0 && state->cy < E.numrows) {
        erow *row = &E.row[E.cy];
        editorRowAppendString(&E.row[state->cy], row->chars, row->size);
        editorDelRow(E.cy);
      }
      E.cy = state->cy;
      E.cx = state->cx;
      break;
      
    case UNDO_DELETE_NEWLINE:
      // Undo newline deletion by splitting the line
      E.cy = state->cy;
      E.cx = state->cx;
      if (E.cy < E.numrows) {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[state->cx], row->size - state->cx);
        row = &E.row[E.cy];
        row->size = state->cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
      }
      break;
      
    case UNDO_DELETE_SELECTION:
      // Restore deleted selection
      if (state->num_lines == 1) {
        E.cy = state->sel_start_cy;
        E.cx = state->sel_start_cx;
        if (E.cy < E.numrows) {
          for (int i = 0; state->lines[0][i]; i++) {
            editorRowInsertChar(&E.row[E.cy], E.cx + i, state->lines[0][i]);
          }
        }
      } else {
        // Multi-line restoration
        E.cy = state->sel_start_cy;
        E.cx = state->sel_start_cx;
        if (E.cy < E.numrows) {
          // Split current line
          erow *row = &E.row[E.cy];
          char *saved_end = NULL;
          int saved_len = 0;
          if (E.cx < row->size) {
            saved_len = row->size - E.cx;
            saved_end = malloc(saved_len + 1);
            memcpy(saved_end, &row->chars[E.cx], saved_len);
            saved_end[saved_len] = '\0';
            row->size = E.cx;
            row->chars[row->size] = '\0';
            editorUpdateRow(row);
          }
          
          // Insert first line fragment
          for (int i = 0; state->lines[0][i]; i++) {
            editorRowInsertChar(&E.row[E.cy], E.cx + i, state->lines[0][i]);
          }
          
          // Insert middle complete lines
          for (int i = 1; i < state->num_lines - 1; i++) {
            editorInsertRow(E.cy + i, state->lines[i], strlen(state->lines[i]));
          }
          
          // Insert last line
          int last_idx = state->num_lines - 1;
          editorInsertRow(E.cy + last_idx, state->lines[last_idx], strlen(state->lines[last_idx]));
          
          // Append saved end
          if (saved_end) {
            editorRowAppendString(&E.row[E.cy + last_idx], saved_end, saved_len);
            free(saved_end);
          }
        }
      }
      E.cy = state->prev_cy;
      E.cx = state->prev_cx;
      break;
  }
  
  E.undo_current = state->prev;
  E.in_undo = 0;
  editorSetStatusMessage("Undo");
}

void editorRedo() {
  if (!E.undo_current) {
    if (!E.undo_head) {
      editorSetStatusMessage("Nothing to redo");
      return;
    }
    E.undo_current = E.undo_head;
  } else if (!E.undo_current->next) {
    editorSetStatusMessage("Nothing to redo");
    return;
  } else {
    E.undo_current = E.undo_current->next;
  }
  
  E.in_undo = 1;
  undoState *state = E.undo_current;
  
  switch (state->type) {
    case UNDO_INSERT_CHAR:
      // Redo character insertion
      E.cy = state->cy;
      E.cx = state->cx;
      if (E.cy >= E.numrows) {
        editorInsertRow(E.numrows, "", 0);
      }
      editorRowInsertChar(&E.row[E.cy], state->cx, state->c);
      E.cx++;
      break;
      
    case UNDO_DELETE_CHAR:
      // Redo character deletion
      E.cy = state->cy;
      E.cx = state->cx + 1;
      if (E.cy < E.numrows) {
        editorRowDelChar(&E.row[E.cy], state->cx);
      }
      E.cx = state->cx;
      break;
      
    case UNDO_INSERT_NEWLINE:
      // Redo newline insertion
      E.cy = state->cy;
      E.cx = state->cx;
      if (E.cy < E.numrows) {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
      } else {
        editorInsertRow(E.cy, "", 0);
      }
      E.cy++;
      E.cx = 0;
      break;
      
    case UNDO_DELETE_NEWLINE:
      // Redo newline deletion
      E.cy = state->cy + 1;
      if (E.cy < E.numrows && state->cy >= 0 && state->cy < E.numrows) {
        erow *row = &E.row[E.cy];
        editorRowAppendString(&E.row[state->cy], row->chars, row->size);
        editorDelRow(E.cy);
      }
      E.cy = state->cy;
      E.cx = state->cx;
      break;
      
    case UNDO_DELETE_SELECTION:
      // Redo selection deletion
      E.cy = state->sel_start_cy;
      E.cx = state->sel_start_cx;
      
      if (state->num_lines == 1) {
        if (E.cy < E.numrows) {
          erow *row = &E.row[E.cy];
          int len = state->sel_end_cx - state->sel_start_cx;
          memmove(&row->chars[state->sel_start_cx], 
                  &row->chars[state->sel_end_cx], 
                  row->size - state->sel_end_cx + 1);
          row->size -= len;
          editorUpdateRow(row);
        }
      } else {
        if (E.cy < E.numrows) {
          erow *start_row = &E.row[state->sel_start_cy];
          erow *end_row = &E.row[state->sel_end_cy];
          int end_len = end_row->size - state->sel_end_cx;
          start_row->chars = realloc(start_row->chars, state->sel_start_cx + end_len + 1);
          memcpy(&start_row->chars[state->sel_start_cx], 
                 &end_row->chars[state->sel_end_cx], end_len);
          start_row->size = state->sel_start_cx + end_len;
          start_row->chars[start_row->size] = '\0';
          
          for (int i = state->sel_end_cy; i > state->sel_start_cy; i--) {
            editorDelRow(i);
          }
          editorUpdateRow(&E.row[state->sel_start_cy]);
        }
      }
      break;
  }
  
  E.in_undo = 0;
  editorSetStatusMessage("Redo");
}

void editorDeleteSelection();
void editorInsertChar(int c) {
  if (E.selection_active) editorDeleteSelection();
  if (E.cy == E.numrows) editorInsertRow(E.numrows, "", 0);
  
  // Save undo state
  undoPush(UNDO_INSERT_CHAR, E.cy, E.cx, c, NULL, 0);
  
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}
void editorInsertNewline() {
  if (E.selection_active) editorDeleteSelection();
  
  // Save undo state
  undoPush(UNDO_INSERT_NEWLINE, E.cy, E.cx, 0, NULL, 0);
  
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++; E.cx = 0;
}
void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    // Save undo state
    char deleted_char = row->chars[E.cx - 1];
    undoPush(UNDO_DELETE_CHAR, E.cy, E.cx - 1, deleted_char, NULL, 0);
    
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    // Save undo state
    undoPush(UNDO_DELETE_NEWLINE, E.cy - 1, E.row[E.cy - 1].size, 0, NULL, 0);
    
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}
void editorNormalizeSelection() {
    if (!E.selection_active) return;
    if (E.sel_end_cy < E.sel_start_cy || 
        (E.sel_end_cy == E.sel_start_cy && E.sel_end_cx < E.sel_start_cx)) {
        int temp_cy = E.sel_start_cy; int temp_cx = E.sel_start_cx;
        E.sel_start_cy = E.sel_end_cy; E.sel_start_cx = E.sel_end_cx;
        E.sel_end_cy = temp_cy; E.sel_end_cx = temp_cx;
    }
}
void editorClearSelection() { E.selection_active = 0; }
void editorDeleteSelection() {
    if (!E.selection_active) return;
    editorNormalizeSelection();
    
    // Save undo state
    undoPushSelectionDeletion();
    
    E.cy = E.sel_start_cy; E.cx = E.sel_start_cx;
    erow *start_row = &E.row[E.sel_start_cy];
    erow *end_row = &E.row[E.sel_end_cy];
    if (E.sel_start_cy == E.sel_end_cy) {
        int len = E.sel_end_cx - E.sel_start_cx;
        if (len > 0) {
            memmove(&start_row->chars[E.sel_start_cx], &start_row->chars[E.sel_end_cx], start_row->size - E.sel_end_cx + 1);
            start_row->size -= len;
            editorUpdateRow(start_row);
        }
    } else { 
        int end_len = end_row->size - E.sel_end_cx;
        start_row->chars = realloc(start_row->chars, E.sel_start_cx + end_len + 1);
        memcpy(&start_row->chars[E.sel_start_cx], &end_row->chars[E.sel_end_cx], end_len);
        start_row->size = E.sel_start_cx + end_len;
        start_row->chars[start_row->size] = '\0';
        for (int i = E.sel_end_cy; i > E.sel_start_cy; i--) {
            editorDelRow(i);
        }
        editorUpdateRow(&E.row[E.sel_start_cy]);
    }
    E.dirty++; editorClearSelection();
}
char *editorRowsToString(int *buflen) {
  int totlen = 0;
  for (int j = 0; j < E.numrows; j++) totlen += E.row[j].size + 1;
  *buflen = totlen;
  char *buf = malloc(totlen); char *p = buf;
  for (int j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size; *p = '\n'; p++;
  }
  return buf;
}
void editorOpen(char *filename) {
  free(E.filename); E.filename = strdup(filename);
  editorSelectSyntaxHighlight();
  FILE *fp = fopen(filename, "r");
  if (!fp) { if (errno != ENOENT) die("fopen"); return; }
  char *line = NULL; size_t linecap = 0; ssize_t linelen;
  
  // Don't record undo for initial file load
  E.in_undo = 1;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  E.in_undo = 0;
  
  free(line); fclose(fp); E.dirty = 0;
}
void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save As: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) { editorSetStatusMessage("Save aborted"); return; }
    editorSelectSyntaxHighlight();
  }
  int len; char *buf = editorRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1 && write(fd, buf, len) == len) {
      close(fd); free(buf); E.dirty = 0;
      editorSetStatusMessage("%d bytes written to disk", len);
      return;
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}
void editorFindCallback(char *query, int key) {
  static int last_match = -1, direction = 1, saved_hl_line;
  static char *saved_hl = NULL;
  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl); saved_hl = NULL;
  }
  if (key == '\r' || key == '\x1b') { last_match = -1; direction = 1; return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) { direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) { direction = -1;
  } else { last_match = -1; direction = 1; }
  if (last_match == -1) direction = 1;
  int current = last_match;
  for (int i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1) current = E.numrows - 1;
    else if (current == E.numrows) current = 0;
    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current; E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;
      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}
void editorFind() {
  int saved_cx = E.cx, saved_cy = E.cy;
  int saved_coloff = E.coloff, saved_rowoff = E.rowoff;
  char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
  if (query) { free(query);
  } else {
    E.cx = saved_cx; E.cy = saved_cy;
    E.coloff = saved_coloff; E.rowoff = saved_rowoff;
  }
}
struct abuf { char *b; int len; };
#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new; ab->len += len;
}
void abFree(struct abuf *ab) { free(ab->b); }
void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  if (E.cy < E.rowoff) E.rowoff = E.cy;
  if (E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
  if (E.rx < E.coloff) E.coloff = E.rx;
  if (E.rx >= E.coloff + E.editor_width) E.coloff = E.rx - E.editor_width + 1;
}
void editorDrawRows(struct abuf *ab) {
  editorNormalizeSelection(); 
  for (int y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0) {
        char *welcome_lines[] = {
            "K 8 O 4", "", "A terminal editor.", "",
            "Ctrl-S: Save | Ctrl-X: Quit | Ctrl-Z: Undo | Ctrl-Y: Redo"
        };
        int num_lines = sizeof(welcome_lines)/sizeof(welcome_lines[0]);
        int start_y = E.screenrows / 3;
        if (y >= start_y && (y - start_y) < num_lines) {
            char *line = welcome_lines[y - start_y];
            int welcomelen = strlen(line);
            int padding = (E.editor_width - welcomelen) / 2;
            if (padding > 0) {
                for (int i=0; i < padding; i++) abAppend(ab, " ", 1);
            }
            abAppend(ab, line, welcomelen);
        }
      }
    } else {
      char buf[16];
      if (filerow == E.cy) abAppend(ab, COLOR_LINENO_CURRENT, strlen(COLOR_LINENO_CURRENT));
      else abAppend(ab, COLOR_LINENO, strlen(COLOR_LINENO));
      snprintf(buf, sizeof(buf), "%4d ", filerow + 1);
      abAppend(ab, buf, strlen(buf));
      abAppend(ab, COLOR_BG, strlen(COLOR_BG));
      erow *row = &E.row[filerow];
      int len = row->rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.editor_width - 5) len = E.editor_width - 5;
      char *c = &row->render[E.coloff]; unsigned char *hl = &row->hl[E.coloff];
      const char* current_color = COLOR_FG;
      abAppend(ab, current_color, strlen(current_color));
      int in_selection = 0;
      for (int j = 0; j < len; j++) {
        int is_selected = 0;
        if (E.selection_active) {
            int current_cx = editorRowRxToCx(row, E.coloff + j);
            if (filerow > E.sel_start_cy && filerow < E.sel_end_cy) is_selected = 1;
            else if (filerow == E.sel_start_cy && filerow == E.sel_end_cy) {
                if (current_cx >= E.sel_start_cx && current_cx < E.sel_end_cx) is_selected = 1;
            } else if (filerow == E.sel_start_cy) {
                if (current_cx >= E.sel_start_cx) is_selected = 1;
            } else if (filerow == E.sel_end_cy) {
                if (current_cx < E.sel_end_cx) is_selected = 1;
            }
        }
        const char *color = editorSyntaxToAnsiColor(hl[j]);
        if (is_selected && !in_selection) {
            abAppend(ab, COLOR_SELECTION_BG, strlen(COLOR_SELECTION_BG));
            abAppend(ab, color, strlen(color));
            in_selection = 1;
        } else if (!is_selected && in_selection) {
            abAppend(ab, COLOR_RESET, strlen(COLOR_RESET));
            abAppend(ab, COLOR_BG, strlen(COLOR_BG));
            abAppend(ab, color, strlen(color)); 
            in_selection = 0;
        }
        if (strcmp(color, current_color)) {
            current_color = color;
            if (!in_selection) abAppend(ab, color, strlen(color));
        }
        abAppend(ab, &c[j], 1);
      }
      abAppend(ab, COLOR_RESET, strlen(COLOR_RESET));
    }
    abAppend(ab, "\x1b[K", 3); abAppend(ab, "\r\n", 2);
  }
}
void editorDrawSidebar(struct abuf *ab) {
    if (!E.sidebar_visible) return;
    char buf[32];
    for (int y = 0; y < E.screenrows; y++) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 2, E.editor_width + 1);
        abAppend(ab, buf, strlen(buf));
        abAppend(ab, COLOR_SIDEBAR_BORDER, strlen(COLOR_SIDEBAR_BORDER));
        abAppend(ab, "│", 3);
    }
    DIR *d = opendir(".");
    if (d) {
        struct dirent *dir; int y = 0;
        snprintf(buf, sizeof(buf), "\x1b[2;%dH", E.editor_width + 3);
        abAppend(ab, buf, strlen(buf));
        abAppend(ab, COLOR_FG, strlen(COLOR_FG));
        abAppend(ab, "EXPLORER", 8);
        while ((dir = readdir(d)) != NULL && y < E.screenrows - 2) {
            if (dir->d_name[0] == '.') continue;
            snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 4, E.editor_width + 3);
            abAppend(ab, buf, strlen(buf));
            char entry[100];
            int len = snprintf(entry, sizeof(entry), "%s", dir->d_name);
            int sidebar_content_width = E.screencols - E.editor_width - 2;
            if (len > sidebar_content_width) len = sidebar_content_width;
            abAppend(ab, entry, len); y++;
        }
        closedir(d);
    }
}
void editorDrawTitleBar(struct abuf *ab) {
  abAppend(ab, COLOR_TITLE_BG, strlen(COLOR_TITLE_BG));
  abAppend(ab, COLOR_STATUS_FG, strlen(COLOR_STATUS_FG));
  char title[E.screencols + 1];
  snprintf(title, sizeof(title), " k8o4 — %s", E.filename ? E.filename : "[Untitled]");
  int len = strlen(title); if (len > E.screencols) len = E.screencols;
  int padding = (E.screencols - len) / 2;
  for (int i = 0; i < padding; i++) abAppend(ab, " ", 1);
  abAppend(ab, title, len);
  while (len + padding < E.screencols) { abAppend(ab, " ", 1); len++; }
  abAppend(ab, COLOR_RESET, strlen(COLOR_RESET)); abAppend(ab, "\r\n", 2);
}
void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, DYNAMIC_COLOR_STATUS_BG, strlen(DYNAMIC_COLOR_STATUS_BG));
  abAppend(ab, COLOR_STATUS_FG, strlen(COLOR_STATUS_FG));
  char status[80];
  int len = snprintf(status, sizeof(status), " NORMAL %s %s",
                     E.filename ? E.filename : "[No Name]", E.dirty ? "●" : "");
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  abAppend(ab, DYNAMIC_COLOR_STATUS_FG_ARROW, strlen(DYNAMIC_COLOR_STATUS_FG_ARROW));
  abAppend(ab, COLOR_STATUS_ALT_BG, strlen(COLOR_STATUS_ALT_BG));
  abAppend(ab, PL_RIGHT_ARROW, strlen(PL_RIGHT_ARROW));
  len++;
  abAppend(ab, COLOR_STATUS_ALT_BG, strlen(COLOR_STATUS_ALT_BG));
  abAppend(ab, COLOR_STATUS_FG, strlen(COLOR_STATUS_FG));
  char rstatus[80];
  int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d:%d ",
                      E.syntax ? E.syntax->filetype : "text", E.cy + 1, E.cx + 1);
  while (len < E.screencols - rlen) { abAppend(ab, " ", 1); len++; }
  abAppend(ab, rstatus, rlen);
  abAppend(ab, COLOR_RESET, strlen(COLOR_RESET)); abAppend(ab, "\r\n", 2);
}
void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}
void editorRefreshScreen() {
  editorScroll();
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6); abAppend(&ab, "\x1b[H", 3);    
  editorDrawTitleBar(&ab); editorDrawRows(&ab);
  if (E.sidebar_visible) editorDrawSidebar(&ab);
  editorDrawStatusBar(&ab); editorDrawMessageBar(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 2,
           (E.rx - E.coloff) + 6); 
  abAppend(&ab, buf, strlen(buf));
  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len); abFree(&ab);
}
void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap); E.statusmsg_time = time(NULL);
}
char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128; char *buf = malloc(bufsize);
  size_t buflen = 0; buf[0] = '\0';
  while (1) {
    editorSetStatusMessage(prompt, buf); editorRefreshScreen();
    int c = editorReadKey();
    if (c == DEL_KEY || c == 127 || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage(""); if (callback) callback(buf, c);
      free(buf); return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage(""); if (callback) callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) { bufsize *= 2; buf = realloc(buf, bufsize); }
      buf[buflen++] = c; buf[buflen] = '\0';
    }
    if (callback) callback(buf, c);
  }
}
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) E.cx--;
    else if (E.cy > 0) { E.cy--; E.cx = E.row[E.cy].size; }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) E.cx++;
    else if (row && E.cx == row->size) { E.cy++; E.cx = 0; }
    break;
  case ARROW_UP: if (E.cy != 0) E.cy--; break;
  case ARROW_DOWN: if (E.cy < E.numrows) E.cy++; break;
  }
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) E.cx = rowlen;
}
void editorMoveCursorWordWise(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch (key) {
        case CTRL_SHIFT_ARROW_LEFT:
            if (E.cx == 0) {
                if (E.cy > 0) { E.cy--; E.cx = E.row[E.cy].size; }
            } else {
                row = &E.row[E.cy];
                E.cx--;
                while (E.cx > 0 && is_separator(row->chars[E.cx])) { E.cx--; }
                while (E.cx > 0 && !is_separator(row->chars[E.cx - 1])) { E.cx--; }
            }
            break;
        case CTRL_SHIFT_ARROW_RIGHT:
            if (!row) break;
            if (E.cx == row->size) {
                if (E.cy < E.numrows - 1) { E.cy++; E.cx = 0; }
            } else {
                while (E.cx < row->size && !is_separator(row->chars[E.cx])) { E.cx++; }
                while (E.cx < row->size && is_separator(row->chars[E.cx])) { E.cx++; }
            }
            break;
    }
}
void editorStartOrExtendSelection(int key) {
    if (!E.selection_active) {
        E.selection_active = 1; E.sel_start_cy = E.cy; E.sel_start_cx = E.cx;
    }
    
    switch(key) {
        case SHIFT_ARROW_UP:    editorMoveCursor(ARROW_UP);    break;
        case SHIFT_ARROW_DOWN:  editorMoveCursor(ARROW_DOWN);  break;
        case SHIFT_ARROW_LEFT:  editorMoveCursor(ARROW_LEFT);  break;
        case SHIFT_ARROW_RIGHT: editorMoveCursor(ARROW_RIGHT); break;
        case SHIFT_HOME_KEY:    E.cx = 0; break;
        case SHIFT_END_KEY:     if (E.cy < E.numrows) E.cx = E.row[E.cy].size; break;
        case CTRL_SHIFT_ARROW_LEFT:
        case CTRL_SHIFT_ARROW_RIGHT:
            editorMoveCursorWordWise(key);
            break;
        default: return; 
    }
    E.sel_end_cy = E.cy; E.sel_end_cx = E.cx;
}
void editorProcessKeypress() {
  static int quit_times = QUIT_TIMES;
  int c = editorReadKey();
  switch (c) {
  case '\r': editorInsertNewline(); break;
  case 24: // Ctrl-X
    if (E.dirty && quit_times > 0) {
      editorSetStatusMessage("WARNING! File has unsaved changes. "
                             "Press Ctrl-X %d more times to quit.", quit_times);
      quit_times--; return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4); write(STDOUT_FILENO, "\x1b[H", 3);
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET)); exit(0);
    break;
  case 1: // Ctrl-A
    if (E.numrows > 0) {
        E.selection_active = 1; E.sel_start_cy = 0; E.sel_start_cx = 0;
        E.sel_end_cy = E.numrows - 1; E.sel_end_cx = E.row[E.numrows - 1].size;
    }
    break;
  case 26: editorUndo(); break; // Ctrl-Z
  case 25: editorRedo(); break; // Ctrl-Y
  case 19: editorSave(); break; // Ctrl-S
  case 6: editorFind(); break; // Ctrl-F
  case 5: // Ctrl-E
    E.sidebar_visible = !E.sidebar_visible;
    E.editor_width = E.screencols - (E.sidebar_visible ? 25 : 5);
    break;
  case HOME_KEY: E.cx = 0; editorClearSelection(); break;
  case END_KEY: if (E.cy < E.numrows) E.cx = E.row[E.cy].size; editorClearSelection(); break;
  
  case BACKSPACE:
    if (E.selection_active) editorDeleteSelection(); else editorDelChar();
    break;
  case DEL_KEY:
    if (E.selection_active) { editorDeleteSelection();
    } else { editorMoveCursor(ARROW_RIGHT); editorDelChar(); }
    break;
  case PAGE_UP: case PAGE_DOWN: {
    editorClearSelection();
    if (c == PAGE_UP) E.cy = E.rowoff;
    else { E.cy = E.rowoff + E.screenrows - 1; if (E.cy > E.numrows) E.cy = E.numrows; }
    int times = E.screenrows;
    while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;
  case ARROW_UP: case ARROW_DOWN: case ARROW_LEFT: case ARROW_RIGHT:
    editorClearSelection(); editorMoveCursor(c);
    break;
  case CTRL_ARROW_LEFT: editorClearSelection(); E.cx = 0; break;
  case CTRL_ARROW_RIGHT:
    editorClearSelection();
    if (E.cy < E.numrows) E.cx = E.row[E.cy].size;
    break;
  case CTRL_ARROW_UP:
  case CTRL_ARROW_DOWN:
    editorClearSelection();
    editorMoveCursor(c == CTRL_ARROW_UP ? ARROW_UP : ARROW_DOWN);
    break;

  case SHIFT_ARROW_UP: case SHIFT_ARROW_DOWN: case SHIFT_ARROW_LEFT: case SHIFT_ARROW_RIGHT:
  case SHIFT_HOME_KEY: case SHIFT_END_KEY:
  case CTRL_SHIFT_ARROW_LEFT: case CTRL_SHIFT_ARROW_RIGHT:
    editorStartOrExtendSelection(c);
    break;
  case 12: case '\x1b': editorClearSelection(); break; // Ctrl-L (clear), ESC
  default: editorInsertChar(c); break;
  }
  quit_times = QUIT_TIMES;
}
void initEditor() {
  E.cx = 0; E.cy = 0; E.rx = 0; E.rowoff = 0; E.coloff = 0; E.numrows = 0;
  E.row = NULL; E.dirty = 0; E.filename = NULL; E.statusmsg[0] = '\0';
  E.statusmsg_time = 0; E.syntax = NULL; E.sidebar_visible = 0;
  E.selection_active = 0;
  E.undo_head = NULL;
  E.undo_current = NULL;
  E.undo_count = 0;
  E.in_undo = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 3;
  E.editor_width = E.screencols - (E.sidebar_visible ? 25 : 5);
}
int main(int argc, char *argv[]) {
  if (!isatty(STDOUT_FILENO)) {
    if (argc < 2) return 1;
    FILE *fp = fopen(argv[1], "r");
    if (!fp) return 1;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
      fwrite(buf, 1, n, stdout);
    }
    fclose(fp);
    return 0;
  }
  enableRawMode(); initEditor();
  srand(time(NULL));
  int r = rand() % 256; int g = rand() % 256; int b = rand() % 256;
  snprintf(DYNAMIC_COLOR_STATUS_BG, sizeof(DYNAMIC_COLOR_STATUS_BG), "\x1b[48;2;%d;%d;%dm", r, g, b);
  snprintf(DYNAMIC_COLOR_STATUS_FG_ARROW, sizeof(DYNAMIC_COLOR_STATUS_FG_ARROW), "\x1b[38;2;%d;%d;%dm", r, g, b);
  if (argc >= 2) { editorOpen(argv[1]); }
  editorSetStatusMessage("HELP: Ctrl-S Save | Ctrl-X Quit | Ctrl-Z Undo | Ctrl-Y Redo");
  while (1) {
    editorRefreshScreen(); editorProcessKeypress();
  }
  return 0;
}
