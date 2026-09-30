#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <process.h>
#include "availabilitymatcher.h"
#include "GroupSync_Colors.h"

#define APP_CLASS L"GroupSyncNativeWindow"
#define GRID_CLASS L"GroupSyncAvailabilityGrid"
#define PARTICIPANTS_CLASS L"GroupSyncParticipantsWindow"
#define MAX_EVENTS 128
#define PAGE_CONTROL_LIMIT 128
#define RESPONSE_TEXT_CAPACITY 8192
#define WM_APP_JOB_DONE (WM_APP + 10)

#define ID_HEADER 100
#define ID_SIDEBAR 101
#define ID_NAV_DASHBOARD 110
#define ID_NAV_CREATE 111
#define ID_NAV_EVENTS 112
#define ID_NAV_SETTINGS 113
#define ID_CARD_DETAILS 200
#define ID_CARD_SHARE 201
#define ID_CARD_AVAILABILITY 202
#define ID_CARD_BEST 203
#define ID_CREATE_EVENT 300
#define ID_ADD_DATE 301
#define ID_REFRESH 302
#define ID_VIEW_PARTICIPANTS 303
#define ID_COPY_LINK 304
#define ID_OPEN_SHEET 305
#define ID_SAVE_SETTINGS 306
#define ID_TEST_CONNECTION 307
#define ID_EVENT_LIST 308
#define ID_CANCEL_EVENT 309
#define ID_REMOVE_DATE_BASE 400

#define PAGE_DASHBOARD 0
#define PAGE_CREATE 1
#define PAGE_EVENTS 2
#define PAGE_SETTINGS 3
#define PAGE_COUNT 4

typedef EventDate DateRange;

typedef struct {
	char event_name[MAX_EVENT_NAME];
	char time_zone[128];
	int duration_minutes;
	int expected_responses;
	int date_count;
	DateRange dates[MAX_DATES];
	char form_id[256];
	char responder_url[1024];
	char sheet_url[1024];
	Participant participants[MAX_PARTICIPANTS];
	int participant_count;
	Recommendation recommendations[3];
	int recommendation_count;
	wchar_t directory[MAX_PATH];
} GroupEvent;

typedef struct {
	wchar_t directory[MAX_PATH];
	wchar_t display_name[MAX_EVENT_NAME + 32];
} SavedEvent;

typedef struct {
	HWND date;
	HWND start;
	HWND end;
	HWND remove;
} DateControls;

typedef enum {
	JOB_PING,
	JOB_CREATE,
	JOB_FETCH,
	JOB_CANCEL
} JobKind;

typedef struct {
	HWND window;
	JobKind kind;
	wchar_t helper[MAX_PATH];
	wchar_t config[MAX_PATH];
	wchar_t input[MAX_PATH];
	wchar_t output[MAX_PATH];
	wchar_t form_id[256];
	int expected_responses;
	int exit_code;
	wchar_t message[1024];
} WorkerJob;

typedef struct {
	HWND window;
	HWND page_controls[PAGE_COUNT][PAGE_CONTROL_LIMIT];
	int page_control_count[PAGE_COUNT];
	HWND nav[PAGE_COUNT];
	HWND connection_label;
	HWND dashboard_title;
	HWND dashboard_subtitle;
	HWND detail_duration;
	HWND detail_timezone;
	HWND detail_response_count;
	HWND responder_edit;
	HWND copy_link_button;
	HWND sheet_button;
	HWND cancel_event_button;
	HWND dashboard_status;
	HWND grid;
	HWND recommendation_labels[3];
	HWND provisional_label;
	HWND refresh_button;
	HWND participants_button;
	HWND create_name;
	HWND create_duration;
	HWND create_timezone;
	HWND create_expected;
	HWND create_status;
	HWND add_date_button;
	HWND create_button;
	DateControls date_controls[MAX_DATES];
	int date_row_count;
	HWND event_list;
	HWND events_hint;
	HWND settings_endpoint;
	HWND settings_key;
	HWND settings_status;
	HWND save_settings_button;
	HWND test_connection_button;
	HFONT font;
	HFONT font_bold;
	HFONT font_title;
	HBITMAP logo_bitmap;
	int logo_width;
	int logo_height;
	int dpi;
	int active_page;
	int busy;
	int configured;
	int connected;
	int grid_scroll_x;
	int grid_scroll_y;
	int create_scroll_y;
	int create_scroll_max;
	wchar_t application_dir[MAX_PATH];
	wchar_t helper_path[MAX_PATH];
	wchar_t data_dir[MAX_PATH];
	wchar_t events_dir[MAX_PATH];
	wchar_t config_path[MAX_PATH];
	SavedEvent saved_events[MAX_EVENTS];
	int saved_event_count;
	GroupEvent current;
	GroupEvent pending;
	int has_event;
} AppState;

static AppState g_app;

static void DefaultTimeZone(wchar_t *output, size_t capacity);
static void LayoutApp(AppState *app);
static void RefreshEventList(AppState *app);

static int CopyWide(wchar_t *destination, size_t capacity, const wchar_t *source)
{
	if (capacity == 0) return 0;
	if (source == NULL) source = L"";
	if (wcslen(source) >= capacity) return 0;
	wcscpy_s(destination, capacity, source);
	return 1;
}

static int CopyUtf8(char *destination, size_t capacity, const char *source)
{
	if (capacity == 0) return 0;
	if (source == NULL) source = "";
	if (strlen(source) >= capacity) return 0;
	strcpy_s(destination, capacity, source);
	return 1;
}

static int WideToUtf8(const wchar_t *source, char *destination, size_t capacity)
{
	int required;
	if (capacity == 0) return 0;
	required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
	                               destination, (int)capacity, NULL, NULL);
	return required > 0;
}

static int Utf8ToWide(const char *source, wchar_t *destination, size_t capacity)
{
	int required;
	if (capacity == 0) return 0;
	required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
	                              destination, (int)capacity);
	return required > 0;
}

static int Scale(const AppState *app, int value)
{
	return MulDiv(value, app->dpi ? app->dpi : 96, 96);
}

static void JoinPath(wchar_t *output, size_t capacity, const wchar_t *left,
                     const wchar_t *right)
{
	_snwprintf_s(output, capacity, _TRUNCATE, L"%ls\\%ls", left, right);
}

static int EnsureDirectory(const wchar_t *path)
{
	if (CreateDirectoryW(path, NULL)) return 1;
	return GetLastError() == ERROR_ALREADY_EXISTS;
}

static int ReadWholeFile(const wchar_t *path, char **contents, size_t *length)
{
	HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
	                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD size;
	DWORD read_count;
	char *buffer;
	if (file == INVALID_HANDLE_VALUE) return 0;
	size = GetFileSize(file, NULL);
	if (size == INVALID_FILE_SIZE || size > 4 * 1024 * 1024) {
		CloseHandle(file);
		return 0;
	}
	buffer = (char *)calloc((size_t)size + 1, 1);
	if (buffer == NULL) {
		CloseHandle(file);
		return 0;
	}
	if (!ReadFile(file, buffer, size, &read_count, NULL) || read_count != size) {
		free(buffer);
		CloseHandle(file);
		return 0;
	}
	CloseHandle(file);
	*contents = buffer;
	if (length != NULL) *length = size;
	return 1;
}

static int WriteUtf8File(const wchar_t *path, const char *contents)
{
	HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
	                          FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD bytes_written;
	DWORD bytes = (DWORD)strlen(contents);
	if (file == INVALID_HANDLE_VALUE) return 0;
	if (!WriteFile(file, contents, bytes, &bytes_written, NULL) || bytes_written != bytes) {
		CloseHandle(file);
		return 0;
	}
	CloseHandle(file);
	return 1;
}

static int AppendUtf8CodePoint(char *output, size_t capacity, size_t *length,
                               unsigned int code_point)
{
	unsigned char bytes[4];
	int count;
	if (code_point <= 0x7F) {
		bytes[0] = (unsigned char)code_point;
		count = 1;
	} else if (code_point <= 0x7FF) {
		bytes[0] = (unsigned char)(0xC0 | (code_point >> 6));
		bytes[1] = (unsigned char)(0x80 | (code_point & 0x3F));
		count = 2;
	} else if (code_point <= 0xFFFF) {
		bytes[0] = (unsigned char)(0xE0 | (code_point >> 12));
		bytes[1] = (unsigned char)(0x80 | ((code_point >> 6) & 0x3F));
		bytes[2] = (unsigned char)(0x80 | (code_point & 0x3F));
		count = 3;
	} else {
		bytes[0] = (unsigned char)(0xF0 | (code_point >> 18));
		bytes[1] = (unsigned char)(0x80 | ((code_point >> 12) & 0x3F));
		bytes[2] = (unsigned char)(0x80 | ((code_point >> 6) & 0x3F));
		bytes[3] = (unsigned char)(0x80 | (code_point & 0x3F));
		count = 4;
	}
	if (*length + (size_t)count >= capacity) return 0;
	memcpy(output + *length, bytes, (size_t)count);
	*length += (size_t)count;
	output[*length] = '\0';
	return 1;
}

static const char *FindJsonValue(const char *json, const char *key)
{
	char needle[128];
	const char *position;
	if (_snprintf_s(needle, sizeof(needle), _TRUNCATE, "\"%s\"", key) < 0) return NULL;
	position = strstr(json, needle);
	if (position == NULL) return NULL;
	position += strlen(needle);
	while (*position && isspace((unsigned char)*position)) ++position;
	if (*position++ != ':') return NULL;
	while (*position && isspace((unsigned char)*position)) ++position;
	return position;
}

static int JsonReadStringAt(const char *position, char *output, size_t capacity)
{
	size_t length = 0;
	if (capacity == 0 || *position++ != '"') return 0;
	output[0] = '\0';
	while (*position && *position != '"') {
		unsigned int value;
		if (*position != '\\') {
			if (length + 1 >= capacity) return 0;
			output[length++] = *position++;
			output[length] = '\0';
			continue;
		}
		++position;
		switch (*position++) {
		case '"': value = '"'; break;
		case '\\': value = '\\'; break;
		case '/': value = '/'; break;
		case 'b': value = '\b'; break;
		case 'f': value = '\f'; break;
		case 'n': value = '\n'; break;
		case 'r': value = '\r'; break;
		case 't': value = '\t'; break;
		case 'u': {
			char hex[5] = { 0 };
			int i;
			for (i = 0; i < 4; ++i) {
				if (!isxdigit((unsigned char)position[i])) return 0;
				hex[i] = position[i];
			}
			value = (unsigned int)strtoul(hex, NULL, 16);
			position += 4;
			break;
		}
		default: return 0;
		}
		if (value < 0x80) {
			if (length + 1 >= capacity) return 0;
			output[length++] = (char)value;
			output[length] = '\0';
		} else if (!AppendUtf8CodePoint(output, capacity, &length, value)) {
			return 0;
		}
	}
	return *position == '"';
}

static int JsonReadString(const char *json, const char *key, char *output,
                          size_t capacity)
{
	const char *position = FindJsonValue(json, key);
	return position != NULL && JsonReadStringAt(position, output, capacity);
}

static int JsonReadInt(const char *json, const char *key, int *value)
{
	const char *position = FindJsonValue(json, key);
	char *end;
	long parsed;
	if (position == NULL) return 0;
	parsed = strtol(position, &end, 10);
	if (end == position || parsed < INT_MIN || parsed > INT_MAX) return 0;
	*value = (int)parsed;
	return 1;
}

static int JsonWriteString(FILE *file, const char *value)
{
	const unsigned char *character = (const unsigned char *)value;
	if (fputc('"', file) == EOF) return 0;
	while (*character) {
		switch (*character) {
		case '"': fputs("\\\"", file); break;
		case '\\': fputs("\\\\", file); break;
		case '\n': fputs("\\n", file); break;
		case '\r': fputs("\\r", file); break;
		case '\t': fputs("\\t", file); break;
		default:
			if (*character < 0x20) fprintf(file, "\\u%04x", *character);
			else fputc(*character, file);
			break;
		}
		++character;
	}
	return fputc('"', file) != EOF && !ferror(file);
}

static int WriteEvent(const GroupEvent *event, const wchar_t *path)
{
	FILE *file;
	int index;
	if (_wfopen_s(&file, path, L"wb") != 0 || file == NULL) return 0;
	fprintf(file, "{\"eventName\":");
	if (!JsonWriteString(file, event->event_name)) goto fail;
	fprintf(file, ",\"durationMinutes\":%d,\"expectedResponses\":%d,\"timeZone\":",
	        event->duration_minutes, event->expected_responses);
	if (!JsonWriteString(file, event->time_zone)) goto fail;
	fprintf(file, ",\"dates\":[");
	for (index = 0; index < event->date_count; ++index) {
		const DateRange *date = &event->dates[index];
		if (index) fputc(',', file);
		fprintf(file, "{\"label\":");
		if (!JsonWriteString(file, date->label)) goto fail;
		fprintf(file, ",\"startMinutes\":%d,\"endMinutes\":%d,\"blockCount\":%d}",
		        date->start_minutes, date->end_minutes, date->block_count);
	}
	fprintf(file, "]}\n");
	if (fclose(file) != 0) return 0;
	return 1;
fail:
	fclose(file);
	return 0;
}

static int ParseEventJson(const char *json, GroupEvent *event)
{
	const char *dates;
	int date_index = 0;
	memset(event, 0, sizeof(*event));
	if (!JsonReadString(json, "eventName", event->event_name, sizeof(event->event_name)) ||
	    !JsonReadInt(json, "durationMinutes", &event->duration_minutes) ||
	    !JsonReadInt(json, "expectedResponses", &event->expected_responses)) return 0;
	JsonReadString(json, "timeZone", event->time_zone, sizeof(event->time_zone));
	dates = FindJsonValue(json, "dates");
	if (dates == NULL || *dates++ != '[') return 0;
	while (*dates && *dates != ']' && date_index < MAX_DATES) {
		const char *start;
		const char *end;
		char object[512];
		size_t object_length;
		while (*dates && (isspace((unsigned char)*dates) || *dates == ',')) ++dates;
		if (*dates == ']') break;
		if (*dates != '{') return 0;
		start = dates++;
		while (*dates && *dates != '}') {
			if (*dates == '"') {
				++dates;
				while (*dates && *dates != '"') {
					if (*dates == '\\' && dates[1]) ++dates;
					++dates;
				}
			}
			if (*dates) ++dates;
		}
		if (*dates != '}') return 0;
		end = ++dates;
		object_length = (size_t)(end - start);
		if (object_length >= sizeof(object)) return 0;
		memcpy(object, start, object_length);
		object[object_length] = '\0';
		if (!JsonReadString(object, "label", event->dates[date_index].label,
		                    sizeof(event->dates[date_index].label)) ||
		    !JsonReadInt(object, "startMinutes", &event->dates[date_index].start_minutes) ||
		    !JsonReadInt(object, "endMinutes", &event->dates[date_index].end_minutes)) return 0;
		event->dates[date_index].block_count =
		    (event->dates[date_index].end_minutes - event->dates[date_index].start_minutes) / 30;
		++date_index;
	}
	event->date_count = date_index;
	return date_index > 0 && event->duration_minutes > 0;
}

static void TrimLine(char *line)
{
	size_t length = strlen(line);
	while (length && (line[length - 1] == '\r' || line[length - 1] == '\n')) {
		line[--length] = '\0';
	}
}

static void ReadFormDetails(GroupEvent *event)
{
	wchar_t path[MAX_PATH];
	FILE *file;
	char line[2048];
	JoinPath(path, MAX_PATH, event->directory, L"form-details.txt");
	if (_wfopen_s(&file, path, L"rb") != 0 || file == NULL) return;
	if (fgets(line, sizeof(line), file)) {
		TrimLine(line);
		CopyUtf8(event->form_id, sizeof(event->form_id), line);
	}
	if (fgets(line, sizeof(line), file)) {
		TrimLine(line);
		CopyUtf8(event->responder_url, sizeof(event->responder_url), line);
	}
	if (fgets(line, sizeof(line), file)) {
		TrimLine(line);
		CopyUtf8(event->sheet_url, sizeof(event->sheet_url), line);
	}
	fclose(file);
}

static int ParseResponses(const char *text, GroupEvent *event)
{
	char *buffer;
	char *context = NULL;
	char *line;
	int count = 0;
	buffer = _strdup(text ? text : "");
	if (buffer == NULL) return 0;
	line = strtok_s(buffer, "\r\n", &context);
	while (line && count < MAX_PARTICIPANTS) {
		Participant *participant = &event->participants[count];
		char *field_context = NULL;
		char *field = strtok_s(line, "\t", &field_context);
		int date_index;
		if (field == NULL || *field == '\0') {
			line = strtok_s(NULL, "\r\n", &context);
			continue;
		}
		CopyUtf8(participant->name, sizeof(participant->name), field);
		for (date_index = 0; date_index < event->date_count; ++date_index) {
			int block;
			for (block = 0; block < event->dates[date_index].block_count; ++block) {
				char *status = strtok_s(NULL, "\t", &field_context);
				char value = status ? (char)toupper((unsigned char)status[0]) : 'U';
				participant->availability[date_index][block] =
					value == 'A' ? AVAILABLE : value == 'M' ? MAYBE : UNAVAILABLE;
			}
		}
		++count;
		line = strtok_s(NULL, "\r\n", &context);
	}
	free(buffer);
	event->participant_count = count;
	event->recommendation_count = find_recommendations(
		event->dates, event->date_count, event->participants, event->participant_count,
		event->duration_minutes, event->recommendations, 3);
	return count;
}

static int LoadEventDirectory(const wchar_t *directory, GroupEvent *event)
{
	wchar_t path[MAX_PATH];
	char *json = NULL;
	char *tsv = NULL;
	JoinPath(path, MAX_PATH, directory, L"event.json");
	if (!ReadWholeFile(path, &json, NULL)) return 0;
	if (!ParseEventJson(json, event)) {
		free(json);
		return 0;
	}
	free(json);
	CopyWide(event->directory, MAX_PATH, directory);
	ReadFormDetails(event);
	JoinPath(path, MAX_PATH, directory, L"responses.tsv");
	if (ReadWholeFile(path, &tsv, NULL)) {
		ParseResponses(tsv, event);
		free(tsv);
	}
	return 1;
}

static int ReadConfig(const wchar_t *path, wchar_t *endpoint, size_t endpoint_cap,
                      wchar_t *api_key, size_t api_key_cap)
{
	char *json = NULL;
	char endpoint_utf8[1024];
	char key_utf8[1024];
	int success = 0;
	if (!ReadWholeFile(path, &json, NULL)) return 0;
	if (JsonReadString(json, "endpoint", endpoint_utf8, sizeof(endpoint_utf8)) &&
	    JsonReadString(json, "apiKey", key_utf8, sizeof(key_utf8)) &&
	    Utf8ToWide(endpoint_utf8, endpoint, endpoint_cap) &&
	    Utf8ToWide(key_utf8, api_key, api_key_cap)) success = 1;
	free(json);
	return success;
}

static int SaveConfig(const wchar_t *path, const wchar_t *endpoint,
                      const wchar_t *api_key)
{
	char endpoint_utf8[1024];
	char key_utf8[1024];
	FILE *file;
	if (!WideToUtf8(endpoint, endpoint_utf8, sizeof(endpoint_utf8)) ||
	    !WideToUtf8(api_key, key_utf8, sizeof(key_utf8))) return 0;
	if (_wfopen_s(&file, path, L"wb") != 0 || file == NULL) return 0;
	fputs("{\"endpoint\":", file);
	if (!JsonWriteString(file, endpoint_utf8)) {
		fclose(file);
		return 0;
	}
	fputs(",\"apiKey\":", file);
	if (!JsonWriteString(file, key_utf8)) {
		fclose(file);
		return 0;
	}
	fputs("}\n", file);
	if (fclose(file) != 0) return 0;
	return 1;
}

static void AddPageControl(AppState *app, int page, HWND control)
{
	if (control && page >= 0 && page < PAGE_COUNT &&
	    app->page_control_count[page] < PAGE_CONTROL_LIMIT) {
		app->page_controls[page][app->page_control_count[page]++] = control;
	}
}

static HWND CreatePageControl(AppState *app, int page, DWORD ex_style,
                              const wchar_t *class_name, const wchar_t *text,
                              DWORD style, int control_id)
{
	HWND control = CreateWindowExW(ex_style, class_name, text,
		WS_CHILD | style, 0, 0, 10, 10, app->window,
		(HMENU)(INT_PTR)control_id, GetModuleHandleW(NULL), NULL);
	AddPageControl(app, page, control);
	return control;
}

static HWND CreateLabel(AppState *app, int page, const wchar_t *text, int id,
                       DWORD style)
{
	HWND control = CreatePageControl(app, page, 0, L"STATIC", text,
		SS_LEFT | style, id);
	if (control) SendMessageW(control, WM_SETFONT, (WPARAM)app->font, TRUE);
	return control;
}

static HWND CreateEdit(AppState *app, int page, const wchar_t *text, int id,
                       DWORD style)
{
	HWND control = CreatePageControl(app, page, WS_EX_CLIENTEDGE, L"EDIT", text,
		ES_AUTOHSCROLL | WS_TABSTOP | style, id);
	if (control) SendMessageW(control, WM_SETFONT, (WPARAM)app->font, TRUE);
	return control;
}

static HWND CreateButton(AppState *app, int page, const wchar_t *text, int id)
{
	HWND control = CreatePageControl(app, page, 0, L"BUTTON", text,
		BS_OWNERDRAW | WS_TABSTOP, id);
	if (control) SendMessageW(control, WM_SETFONT, (WPARAM)app->font_bold, TRUE);
	return control;
}

static HWND CreateCard(AppState *app, int page, int id)
{
	return CreatePageControl(app, page, 0, L"STATIC", L"",
		SS_OWNERDRAW, id);
}

static void CreateDateRowControls(AppState *app, int index)
{
	DateControls *row = &app->date_controls[index];
	wchar_t label[32];
	row->date = CreateEdit(app, PAGE_CREATE, L"", 500 + index * 4, 0);
	row->start = CreateEdit(app, PAGE_CREATE, L"09:00", 501 + index * 4, 0);
	row->end = CreateEdit(app, PAGE_CREATE, L"17:00", 502 + index * 4, 0);
	_snwprintf_s(label, 32, _TRUNCATE, L"Remove");
	row->remove = CreateButton(app, PAGE_CREATE, label, ID_REMOVE_DATE_BASE + index);
	if (row->date) SendMessageW(row->date, EM_SETLIMITTEXT, MAX_DATE_LABEL - 1, 0);
	if (row->start) SendMessageW(row->start, EM_SETLIMITTEXT, 5, 0);
	if (row->end) SendMessageW(row->end, EM_SETLIMITTEXT, 5, 0);
}

static void CreateControls(AppState *app)
{
	int index;
	app->font = CreateFontW(-MulDiv(9, app->dpi ? app->dpi : 96, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	app->font_bold = CreateFontW(-MulDiv(9, app->dpi ? app->dpi : 96, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	app->font_title = CreateFontW(-MulDiv(20, app->dpi ? app->dpi : 96, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

	HWND header = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
		0, 0, 10, 10, app->window, (HMENU)ID_HEADER, GetModuleHandleW(NULL), NULL);
	HWND sidebar = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
		0, 0, 10, 10, app->window, (HMENU)ID_SIDEBAR, GetModuleHandleW(NULL), NULL);
	(void)header;
	(void)sidebar;

	app->nav[PAGE_DASHBOARD] = CreateWindowExW(0, L"BUTTON", L"Dashboard",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, app->window,
		(HMENU)ID_NAV_DASHBOARD, GetModuleHandleW(NULL), NULL);
	app->nav[PAGE_CREATE] = CreateWindowExW(0, L"BUTTON", L"Create Event",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, app->window,
		(HMENU)ID_NAV_CREATE, GetModuleHandleW(NULL), NULL);
	app->nav[PAGE_EVENTS] = CreateWindowExW(0, L"BUTTON", L"My Events",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, app->window,
		(HMENU)ID_NAV_EVENTS, GetModuleHandleW(NULL), NULL);
	app->nav[PAGE_SETTINGS] = CreateWindowExW(0, L"BUTTON", L"Settings",
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 10, 10, app->window,
		(HMENU)ID_NAV_SETTINGS, GetModuleHandleW(NULL), NULL);
	for (index = 0; index < PAGE_COUNT; ++index) {
		SendMessageW(app->nav[index], WM_SETFONT, (WPARAM)app->font_bold, TRUE);
	}
	app->connection_label = CreateWindowExW(0, L"STATIC", L"Checking connection...",
		WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE, 0, 0, 10, 10,
		app->window, (HMENU)102, GetModuleHandleW(NULL), NULL);
	SendMessageW(app->connection_label, WM_SETFONT, (WPARAM)app->font_bold, TRUE);

	CreateCard(app, PAGE_DASHBOARD, ID_CARD_DETAILS);
	CreateCard(app, PAGE_DASHBOARD, ID_CARD_SHARE);
	CreateCard(app, PAGE_DASHBOARD, ID_CARD_AVAILABILITY);
	CreateCard(app, PAGE_DASHBOARD, ID_CARD_BEST);
	app->dashboard_title = CreateLabel(app, PAGE_DASHBOARD, L"No event selected", 600, 0);
	app->dashboard_subtitle = CreateLabel(app, PAGE_DASHBOARD,
		L"Find a time that works for everyone.", 601, 0);
	CreateLabel(app, PAGE_DASHBOARD, L"Event Details", 602, 0);
	app->detail_duration = CreateLabel(app, PAGE_DASHBOARD, L"Meeting length: --", 603, 0);
	app->detail_timezone = CreateLabel(app, PAGE_DASHBOARD, L"Time zone: --", 604, 0);
	app->detail_response_count = CreateLabel(app, PAGE_DASHBOARD,
		L"Responses: 0 / 0", 605, 0);
	CreateLabel(app, PAGE_DASHBOARD, L"Share Event", 606, 0);
	app->responder_edit = CreateEdit(app, PAGE_DASHBOARD, L"", 607, ES_READONLY);
	app->copy_link_button = CreateButton(app, PAGE_DASHBOARD, L"Copy Link", ID_COPY_LINK);
	app->sheet_button = CreateButton(app, PAGE_DASHBOARD, L"Open Response Sheet", ID_OPEN_SHEET);
	app->cancel_event_button = CreateButton(app, PAGE_DASHBOARD, L"Cancel Event", ID_CANCEL_EVENT);
	CreateLabel(app, PAGE_DASHBOARD, L"Group Availability", 608, 0);
	CreateLabel(app, PAGE_DASHBOARD,
		L"Available / received responses  |  30-minute blocks", 609, 0);
	CreateLabel(app, PAGE_DASHBOARD, L"All available", 610, SS_OWNERDRAW);
	CreateLabel(app, PAGE_DASHBOARD, L"Some available", 611, SS_OWNERDRAW);
	CreateLabel(app, PAGE_DASHBOARD, L"None available", 612, SS_OWNERDRAW);
	app->grid = CreateWindowExW(WS_EX_CLIENTEDGE, GRID_CLASS, L"",
		WS_CHILD | WS_TABSTOP | WS_HSCROLL | WS_VSCROLL, 0, 0, 10, 10,
		app->window, NULL, GetModuleHandleW(NULL), app);
	AddPageControl(app, PAGE_DASHBOARD, app->grid);
	CreateLabel(app, PAGE_DASHBOARD, L"Best Meeting Times", 615, 0);
	app->provisional_label = CreateLabel(app, PAGE_DASHBOARD, L"", 613, 0);
	for (index = 0; index < 3; ++index) {
		app->recommendation_labels[index] = CreateLabel(app, PAGE_DASHBOARD, L"", 620 + index, 0);
	}
	app->refresh_button = CreateButton(app, PAGE_DASHBOARD, L"Refresh Responses", ID_REFRESH);
	app->participants_button = CreateButton(app, PAGE_DASHBOARD,
		L"View Participants", ID_VIEW_PARTICIPANTS);
	app->dashboard_status = CreateLabel(app, PAGE_DASHBOARD,
		L"Create an event to get started.", 614, 0);

	CreateLabel(app, PAGE_CREATE, L"Create Event", 700, 0);
	CreateLabel(app, PAGE_CREATE, L"Event name", 701, 0);
	app->create_name = CreateEdit(app, PAGE_CREATE, L"", 702, 0);
	SendMessageW(app->create_name, EM_SETLIMITTEXT, MAX_EVENT_NAME - 1, 0);
	CreateLabel(app, PAGE_CREATE, L"Meeting duration (minutes)", 703, 0);
	app->create_duration = CreateEdit(app, PAGE_CREATE, L"60", 704, ES_NUMBER);
	SendMessageW(app->create_duration, EM_SETLIMITTEXT, 4, 0);
	CreateLabel(app, PAGE_CREATE, L"Organizer time-zone label", 705, 0);
	app->create_timezone = CreateEdit(app, PAGE_CREATE, L"", 706, 0);
	SendMessageW(app->create_timezone, EM_SETLIMITTEXT, 60, 0);
	{
		wchar_t time_zone[128];
		DefaultTimeZone(time_zone, _countof(time_zone));
		SetWindowTextW(app->create_timezone, time_zone);
	}
	CreateLabel(app, PAGE_CREATE, L"Expected participants", 707, 0);
	app->create_expected = CreateEdit(app, PAGE_CREATE, L"5", 708, ES_NUMBER);
	SendMessageW(app->create_expected, EM_SETLIMITTEXT, 2, 0);
	CreateLabel(app, PAGE_CREATE, L"Possible dates and time ranges", 709, 0);
	CreateLabel(app, PAGE_CREATE,
		L"Use YYYY-MM-DD dates and 24-hour times on 30-minute boundaries. Time zone is a label only.", 710, 0);
	CreateLabel(app, PAGE_CREATE, L"Date", 712, 0);
	CreateLabel(app, PAGE_CREATE, L"Start", 713, 0);
	CreateLabel(app, PAGE_CREATE, L"End", 714, 0);
	app->add_date_button = CreateButton(app, PAGE_CREATE, L"+  Add date", ID_ADD_DATE);
	for (index = 0; index < MAX_DATES; ++index) CreateDateRowControls(app, index);
	app->date_row_count = 1;
	{
		SYSTEMTIME now;
		wchar_t date[32];
		GetLocalTime(&now);
		_snwprintf_s(date, 32, _TRUNCATE, L"%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);
		SetWindowTextW(app->date_controls[0].date, date);
	}
	app->create_button = CreateButton(app, PAGE_CREATE,
		L"Create Event", ID_CREATE_EVENT);
	app->create_status = CreateLabel(app, PAGE_CREATE, L"", 711, 0);

	CreateLabel(app, PAGE_EVENTS, L"My Events", 800, 0);
	CreateLabel(app, PAGE_EVENTS, L"Saved on this computer. Select an event to view its details and results.", 801, 0);
	app->event_list = CreatePageControl(app, PAGE_EVENTS, WS_EX_CLIENTEDGE,
		L"LISTBOX", L"", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP, ID_EVENT_LIST);
	SendMessageW(app->event_list, WM_SETFONT, (WPARAM)app->font, TRUE);
	app->events_hint = CreateLabel(app, PAGE_EVENTS, L"", 802, 0);

	CreateLabel(app, PAGE_SETTINGS, L"Settings", 900, 0);
	CreateLabel(app, PAGE_SETTINGS, L"Apps Script deployment URL", 901, 0);
	app->settings_endpoint = CreateEdit(app, PAGE_SETTINGS, L"", 902, 0);
	SendMessageW(app->settings_endpoint, EM_SETLIMITTEXT, 900, 0);
	CreateLabel(app, PAGE_SETTINGS, L"API key", 903, 0);
	app->settings_key = CreateEdit(app, PAGE_SETTINGS, L"", 904, ES_PASSWORD);
	SendMessageW(app->settings_key, EM_SETLIMITTEXT, 900, 0);
	CreateLabel(app, PAGE_SETTINGS,
		L"Credentials are stored in your Windows user profile and are not written to logs or the project folder.", 905, 0);
	app->save_settings_button = CreateButton(app, PAGE_SETTINGS,
		L"Save Settings", ID_SAVE_SETTINGS);
	app->test_connection_button = CreateButton(app, PAGE_SETTINGS,
		L"Test Connection", ID_TEST_CONNECTION);
	app->settings_status = CreateLabel(app, PAGE_SETTINGS, L"", 906, 0);
}

static void ShowPage(AppState *app, int page)
{
	int current;
	int index;
	if (page < 0 || page >= PAGE_COUNT) return;
	app->active_page = page;
	for (current = 0; current < PAGE_COUNT; ++current) {
		for (index = 0; index < app->page_control_count[current]; ++index) {
			ShowWindow(app->page_controls[current][index], current == page ? SW_SHOW : SW_HIDE);
		}
	}
	for (current = 0; current < PAGE_COUNT; ++current) InvalidateRect(app->nav[current], NULL, TRUE);
	if (page == PAGE_EVENTS) RefreshEventList(app);
	LayoutApp(app);
}

static void SetStatus(HWND control, const wchar_t *message)
{
	if (control) SetWindowTextW(control, message ? message : L"");
}

static void UpdateConnection(AppState *app, int state, const wchar_t *detail)
{
	app->connected = state;
	if (state > 0) SetStatus(app->connection_label, L"\x25CF  Connected to Google");
	else if (state == 0) SetStatus(app->connection_label, L"\x25CF  Checking connection...");
	else if (state == -2) SetStatus(app->connection_label, L"\x25CF  Configure Apps Script");
	else SetStatus(app->connection_label, L"\x25CF  Not connected");
	if (detail && app->settings_status) SetStatus(app->settings_status, detail);
}

static void FormatClock(int minutes, wchar_t *output, size_t capacity)
{
	int hour;
	int minute;
	const wchar_t *period;
	minutes %= 24 * 60;
	if (minutes < 0) minutes += 24 * 60;
	hour = minutes / 60;
	minute = minutes % 60;
	period = hour < 12 ? L"AM" : L"PM";
	hour %= 12;
	if (hour == 0) hour = 12;
	_snwprintf_s(output, capacity, _TRUNCATE, L"%d:%02d %ls", hour, minute, period);
}

static void UpdateDashboard(AppState *app)
{
	wchar_t name[MAX_EVENT_NAME + 1];
	wchar_t value[512];
	wchar_t duration[64];
	wchar_t time_zone[256];
	wchar_t responder[1024];
	int index;
	if (!app->has_event) {
		SetStatus(app->dashboard_title, L"No event selected");
		SetStatus(app->dashboard_subtitle, L"Find a time that works for everyone.");
		SetStatus(app->detail_duration, L"Meeting length: --");
		SetStatus(app->detail_timezone, L"Time zone: --");
		SetStatus(app->detail_response_count, L"Responses: 0 / 0");
		SetWindowTextW(app->responder_edit, L"");
		EnableWindow(app->copy_link_button, FALSE);
		EnableWindow(app->sheet_button, FALSE);
		EnableWindow(app->refresh_button, FALSE);
		EnableWindow(app->participants_button, FALSE);
		EnableWindow(app->cancel_event_button, FALSE);
		SetStatus(app->provisional_label, L"");
		SetStatus(app->recommendation_labels[0], L"Create an event to see suggested meeting times.");
		SetStatus(app->recommendation_labels[1], L"");
		SetStatus(app->recommendation_labels[2], L"");
		SetStatus(app->dashboard_status, L"Create an event to get started.");
		if (app->grid) InvalidateRect(app->grid, NULL, TRUE);
		return;
	}
	Utf8ToWide(app->current.event_name, name, _countof(name));
	_snwprintf_s(value, _countof(value), _TRUNCATE, L"%ls", name);
	SetStatus(app->dashboard_title, value);
	SetStatus(app->dashboard_subtitle, L"Find a time that works for everyone.");
	_snwprintf_s(duration, _countof(duration), _TRUNCATE,
		L"Meeting length: %d minutes", app->current.duration_minutes);
	SetStatus(app->detail_duration, duration);
	Utf8ToWide(app->current.time_zone, time_zone, _countof(time_zone));
	_snwprintf_s(value, _countof(value), _TRUNCATE, L"Time zone: %ls", time_zone);
	SetStatus(app->detail_timezone, value);
	_snwprintf_s(value, _countof(value), _TRUNCATE, L"Responses: %d / %d",
		app->current.participant_count, app->current.expected_responses);
	SetStatus(app->detail_response_count, value);
	if (Utf8ToWide(app->current.responder_url, responder, _countof(responder)))
		SetWindowTextW(app->responder_edit, responder);
	else SetWindowTextW(app->responder_edit, L"");
	EnableWindow(app->copy_link_button, app->current.responder_url[0] != '\0');
	EnableWindow(app->sheet_button, app->current.sheet_url[0] != '\0');
	EnableWindow(app->refresh_button, app->current.form_id[0] != '\0' && !app->busy);
	EnableWindow(app->participants_button, app->current.participant_count > 0);
	EnableWindow(app->cancel_event_button,
		app->current.form_id[0] != '\0' && !app->busy);
	if (app->current.participant_count < app->current.expected_responses) {
		_snwprintf_s(value, _countof(value), _TRUNCATE,
			L"Provisional  \x2022  %d of %d responses received",
			app->current.participant_count, app->current.expected_responses);
		SetStatus(app->provisional_label, value);
	} else {
		SetStatus(app->provisional_label, L"Final responses received");
	}
	if (app->current.participant_count == 0) {
		SetStatus(app->dashboard_status,
			app->current.form_id[0] ? L"No responses yet. Refresh to check for submissions." :
			L"Create an event to get its sharing link and availability grid.");
	} else if (app->current.recommendation_count == 0) {
		SetStatus(app->dashboard_status, L"No suitable meeting windows in the submitted ranges.");
	} else {
		SetStatus(app->dashboard_status, L"");
	}
	for (index = 0; index < 3; ++index) {
		if (index < app->current.recommendation_count) {
			Recommendation *recommendation = &app->current.recommendations[index];
			DateRange *date = &app->current.dates[recommendation->date_index];
			int start = date->start_minutes + recommendation->start_block * 30;
			int end = start + app->current.duration_minutes;
			wchar_t wide_date[MAX_DATE_LABEL];
			wchar_t start_text[48];
			wchar_t end_text[48];
			wchar_t rec_text[512];
			FormatClock(start, start_text, _countof(start_text));
			FormatClock(end, end_text, _countof(end_text));
			Utf8ToWide(date->label, wide_date, _countof(wide_date));
			_snwprintf_s(rec_text, _countof(rec_text), _TRUNCATE,
				L"%d. %ls\r\n%ls - %ls  |  %d available  |  %d maybe",
				index + 1, wide_date, start_text, end_text,
				recommendation->available_count, recommendation->maybe_count);
			SetStatus(app->recommendation_labels[index], rec_text);
		} else {
			SetStatus(app->recommendation_labels[index], L"");
		}
	}
	if (app->current.recommendation_count == 0) {
		SetStatus(app->recommendation_labels[0], app->current.participant_count == 0 ?
			L"No responses yet. Refresh to check again." :
			L"No suitable meeting times fit the submitted availability.");
	}
	if (app->grid) InvalidateRect(app->grid, NULL, TRUE);
}

static void SetControlFont(HWND control, HFONT font)
{
	if (control && font) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static void Place(AppState *app, HWND control, int x, int y, int width, int height)
{
	if (control) MoveWindow(control, Scale(app, x), Scale(app, y),
		Scale(app, width), Scale(app, height), TRUE);
}

static void LayoutApp(AppState *app)
{
	RECT client;
	int width;
	int height;
	int right_x;
	int left_width;
	int legend_width;
	int card_bottom;
	int row;
	int nav_y = 100;
	int create_area_bottom;
	int create_area_height;
	int create_content_height;
	GetClientRect(app->window, &client);
	width = MulDiv(client.right, 96, app->dpi ? app->dpi : 96);
	height = MulDiv(client.bottom, 96, app->dpi ? app->dpi : 96);
	Place(app, GetDlgItem(app->window, ID_HEADER), 0, 0, width, 70);
	Place(app, GetDlgItem(app->window, ID_SIDEBAR), 0, 70, 220, height - 70);
	Place(app, app->nav[PAGE_DASHBOARD], 12, nav_y, 196, 40);
	Place(app, app->nav[PAGE_CREATE], 12, nav_y + 48, 196, 40);
	Place(app, app->nav[PAGE_EVENTS], 12, nav_y + 96, 196, 40);
	Place(app, app->nav[PAGE_SETTINGS], 12, nav_y + 144, 196, 40);
	Place(app, app->connection_label, width - 290, 22, 264, 28);

	right_x = width - 448;
	left_width = right_x - 250 - 24;
	if (left_width < 430) left_width = 430;
	legend_width = (left_width - 40) / 3;
	card_bottom = height - 24;
	Place(app, app->dashboard_title, 250, 98, left_width, 34);
	Place(app, app->dashboard_subtitle, 250, 132, left_width, 24);
	Place(app, GetDlgItem(app->window, 602), 270, 184, 280, 26);
	Place(app, GetDlgItem(app->window, ID_CARD_DETAILS), 250, 166, left_width, 220);
	Place(app, app->detail_duration, 270, 224, left_width / 3, 34);
	Place(app, app->detail_timezone, 270 + left_width / 3, 224, left_width / 3, 34);
	Place(app, app->detail_response_count, 270 + (left_width * 2) / 3, 224, left_width / 3 - 24, 34);
	Place(app, GetDlgItem(app->window, 606), right_x + 20, 184, 300, 26);
	Place(app, GetDlgItem(app->window, ID_CARD_SHARE), right_x, 166, 424, 220);
	Place(app, app->responder_edit, right_x + 20, 224, 384, 32);
	Place(app, app->copy_link_button, right_x + 20, 270, 126, 36);
	Place(app, app->sheet_button, right_x + 158, 270, 246, 36);
	Place(app, app->cancel_event_button, right_x + 20, 322, 150, 34);

	Place(app, GetDlgItem(app->window, ID_CARD_AVAILABILITY), 250, 410,
		left_width, card_bottom - 410);
	Place(app, GetDlgItem(app->window, 608), 270, 428, left_width - 40, 26);
	Place(app, GetDlgItem(app->window, 609), 270, 458, left_width - 40, 22);
	Place(app, GetDlgItem(app->window, 610), 270, 484, legend_width, 24);
	Place(app, GetDlgItem(app->window, 611), 270 + legend_width, 484, legend_width, 24);
	Place(app, GetDlgItem(app->window, 612), 270 + (legend_width * 2), 484, legend_width, 24);
	Place(app, app->grid, 270, 516, left_width - 40, card_bottom - 558);
	Place(app, GetDlgItem(app->window, ID_CARD_BEST), right_x, 410,
		424, card_bottom - 410);
	Place(app, app->refresh_button, right_x + 20, card_bottom - 52, 184, 34);
	Place(app, app->participants_button, right_x + 214, card_bottom - 52, 190, 34);
	Place(app, app->dashboard_status, 270, card_bottom - 34, left_width - 40, 20);
	Place(app, GetDlgItem(app->window, 615), right_x + 20, 428, 384, 26);
	Place(app, app->provisional_label, right_x + 20, 458, 384, 26);
	for (row = 0; row < 3; ++row)
		Place(app, app->recommendation_labels[row], right_x + 20,
			490 + row * 62, 384, 56);

	Place(app, GetDlgItem(app->window, 700), 250, 98, 500, 36);
	Place(app, GetDlgItem(app->window, 701), 250, 158, 200, 22);
	Place(app, app->create_name, 250, 182, 660, 32);
	Place(app, GetDlgItem(app->window, 703), 250, 232, 210, 22);
	Place(app, app->create_duration, 250, 256, 150, 32);
	Place(app, GetDlgItem(app->window, 705), 430, 232, 220, 22);
	Place(app, app->create_timezone, 430, 256, 280, 32);
	Place(app, GetDlgItem(app->window, 707), 740, 232, 210, 22);
	Place(app, app->create_expected, 740, 256, 140, 32);
	Place(app, GetDlgItem(app->window, 709), 250, 310, 450, 24);
	Place(app, GetDlgItem(app->window, 710), 250, 337, width - 520, 22);
	Place(app, GetDlgItem(app->window, 712), 250, 364, 200, 22);
	Place(app, GetDlgItem(app->window, 713), 465, 364, 110, 22);
	Place(app, GetDlgItem(app->window, 714), 590, 364, 110, 22);
	Place(app, app->add_date_button, width - 190, 304, 154, 36);
	create_area_bottom = height - 100;
	create_area_height = create_area_bottom - 390;
	if (create_area_height < 30) create_area_height = 30;
	create_content_height = app->date_row_count * 30;
	app->create_scroll_max = create_content_height > create_area_height ?
		((create_content_height - create_area_height + 29) / 30) * 30 : 0;
	if (app->create_scroll_y > app->create_scroll_max)
		app->create_scroll_y = app->create_scroll_max;
	{
		SCROLLINFO info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		info.nMin = 0;
		info.nMax = app->create_scroll_max + create_area_height - 1;
		info.nPage = (UINT)create_area_height;
		info.nPos = app->create_scroll_y;
		SetScrollInfo(app->window, SB_VERT, &info, TRUE);
		ShowScrollBar(app->window, SB_VERT,
			app->active_page == PAGE_CREATE && app->create_scroll_max > 0);
	}
	for (row = 0; row < MAX_DATES; ++row) {
		int row_y = 390 + row * 30 - app->create_scroll_y;
		int visible = row < app->date_row_count && row_y >= 390 && row_y + 27 <= create_area_bottom;
		if (visible) {
			Place(app, app->date_controls[row].date, 250, row_y, 200, 27);
			Place(app, app->date_controls[row].start, 465, row_y, 110, 27);
			Place(app, app->date_controls[row].end, 590, row_y, 110, 27);
			Place(app, app->date_controls[row].remove, 715, row_y - 1, 110, 29);
		}
		ShowWindow(app->date_controls[row].date, visible ? SW_SHOW : SW_HIDE);
		ShowWindow(app->date_controls[row].start, visible ? SW_SHOW : SW_HIDE);
		ShowWindow(app->date_controls[row].end, visible ? SW_SHOW : SW_HIDE);
		ShowWindow(app->date_controls[row].remove, visible ? SW_SHOW : SW_HIDE);
	}
	Place(app, app->create_button, 250, height - 58, 180, 38);
	Place(app, app->create_status, 450, height - 54, width - 700, 30);

	Place(app, GetDlgItem(app->window, 800), 250, 98, 400, 36);
	Place(app, GetDlgItem(app->window, 801), 250, 140, width - 300, 28);
	Place(app, app->event_list, 250, 184, width - 300, height - 250);
	Place(app, app->events_hint, 250, height - 54, width - 300, 26);

	Place(app, GetDlgItem(app->window, 900), 250, 98, 400, 36);
	Place(app, GetDlgItem(app->window, 901), 250, 164, 500, 24);
	Place(app, app->settings_endpoint, 250, 192, 700, 34);
	Place(app, GetDlgItem(app->window, 903), 250, 250, 300, 24);
	Place(app, app->settings_key, 250, 278, 700, 34);
	Place(app, GetDlgItem(app->window, 905), 250, 326, width - 320, 48);
	Place(app, app->save_settings_button, 250, 394, 150, 38);
	Place(app, app->test_connection_button, 418, 394, 170, 38);
	Place(app, app->settings_status, 250, 450, width - 320, 32);
}

static void DrawRoundRect(HDC dc, const RECT *rectangle, COLORREF fill,
                          COLORREF border, int radius)
{
	HBRUSH brush = CreateSolidBrush(fill);
	HPEN pen = CreatePen(PS_SOLID, 1, border);
	HGDIOBJ old_brush = SelectObject(dc, brush);
	HGDIOBJ old_pen = SelectObject(dc, pen);
	RoundRect(dc, rectangle->left, rectangle->top, rectangle->right,
		rectangle->bottom, radius, radius);
	SelectObject(dc, old_brush);
	SelectObject(dc, old_pen);
	DeleteObject(brush);
	DeleteObject(pen);
}

static void DrawButton(AppState *app, DRAWITEMSTRUCT *item)
{
	wchar_t text[128];
	RECT rect = item->rcItem;
	COLORREF fill = GS_COLOR_SURFACE;
	COLORREF border = GS_COLOR_BORDER;
	COLORREF text_color = GS_COLOR_TEXT;
	int id = (int)item->CtlID;
	GetWindowTextW(item->hwndItem, text, _countof(text));
	if (id >= ID_NAV_DASHBOARD && id <= ID_NAV_SETTINGS) {
		int page = id - ID_NAV_DASHBOARD;
		if (page == app->active_page) {
			fill = GS_COLOR_PRIMARY_BG;
			border = fill;
			text_color = GS_COLOR_PRIMARY;
		} else {
			fill = GS_COLOR_WINDOW_BG;
			border = fill;
			text_color = GS_COLOR_TEXT_SECONDARY;
		}
	} else if (id == ID_CREATE_EVENT || id == ID_COPY_LINK || id == ID_REFRESH ||
	           id == ID_SAVE_SETTINGS || id == ID_TEST_CONNECTION || id == ID_ADD_DATE) {
		fill = (item->itemState & ODS_SELECTED) ? GS_COLOR_PRIMARY_HOVER : GS_COLOR_PRIMARY;
		border = fill;
		text_color = GS_COLOR_BUTTON_TEXT;
	} else if (id == ID_CANCEL_EVENT) {
		fill = GS_COLOR_UNAVAILABLE_BG;
		border = GS_COLOR_UNAVAILABLE;
		text_color = GS_COLOR_UNAVAILABLE;
	}
	if (item->itemState & ODS_DISABLED) {
		fill = GS_COLOR_WINDOW_BG;
		border = GS_COLOR_BORDER;
		text_color = GS_COLOR_TEXT_SECONDARY;
	}
	if (item->itemState & ODS_FOCUS) InflateRect(&rect, -2, -2);
	DrawRoundRect(item->hDC, &rect, fill, border, Scale(app, 7));
	SetBkMode(item->hDC, TRANSPARENT);
	SetTextColor(item->hDC, text_color);
	SelectObject(item->hDC, app->font_bold);
	DrawTextW(item->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &rect);
}

static void DrawLogo(HDC dc, int x, int y, int scale)
{
	HPEN pen = CreatePen(PS_SOLID, scale * 2, GS_COLOR_PRIMARY);
	HBRUSH brush = CreateSolidBrush(GS_COLOR_PRIMARY);
	HGDIOBJ old_pen = SelectObject(dc, pen);
	HGDIOBJ old_brush = SelectObject(dc, brush);
	MoveToEx(dc, x + 8 * scale, y + 9 * scale, NULL);
	LineTo(dc, x + 22 * scale, y + 9 * scale);
	LineTo(dc, x + 22 * scale, y + 23 * scale);
	LineTo(dc, x + 8 * scale, y + 23 * scale);
	Ellipse(dc, x + 4 * scale, y + 4 * scale, x + 12 * scale, y + 12 * scale);
	Ellipse(dc, x + 18 * scale, y + 4 * scale, x + 26 * scale, y + 12 * scale);
	Ellipse(dc, x + 18 * scale, y + 19 * scale, x + 26 * scale, y + 27 * scale);
	Ellipse(dc, x + 4 * scale, y + 19 * scale, x + 12 * scale, y + 27 * scale);
	SelectObject(dc, old_pen);
	SelectObject(dc, old_brush);
	DeleteObject(pen);
	DeleteObject(brush);
}

static int ReadIntControl(HWND control, int minimum, int maximum, int *value)
{
	wchar_t text[32];
	wchar_t *end;
	long parsed;
	GetWindowTextW(control, text, _countof(text));
	parsed = wcstol(text, &end, 10);
	while (*end == L' ' || *end == L'\t') ++end;
	if (end == text || *end || parsed < minimum || parsed > maximum) return 0;
	*value = (int)parsed;
	return 1;
}

static int ParseTimeText(const wchar_t *text, int allow_midnight_end, int *minutes)
{
	int hour;
	int minute;
	if (wcslen(text) != 5 || text[2] != L':' ||
	    !iswdigit(text[0]) || !iswdigit(text[1]) ||
	    !iswdigit(text[3]) || !iswdigit(text[4])) return 0;
	hour = (text[0] - L'0') * 10 + text[1] - L'0';
	minute = (text[3] - L'0') * 10 + text[4] - L'0';
	if (minute != 0 && minute != 30) return 0;
	if (allow_midnight_end && hour == 24 && minute == 0) {
		*minutes = 1440;
		return 1;
	}
	if (hour < 0 || hour > 23) return 0;
	*minutes = hour * 60 + minute;
	return 1;
}

static int ParseDateText(const wchar_t *text)
{
	int year;
	int month;
	int day;
	int days_in_month;
	int leap;
	static const int month_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (wcslen(text) != 10 || text[4] != L'-' || text[7] != L'-') return 0;
	if (swscanf_s(text, L"%4d-%2d-%2d", &year, &month, &day) != 3) return 0;
	if (year < 1601 || month < 1 || month > 12) return 0;
	leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
	days_in_month = month_days[month - 1] + (month == 2 && leap);
	return day >= 1 && day <= days_in_month;
}

static void QuoteArgument(wchar_t *command, size_t capacity, size_t *length,
                          const wchar_t *argument)
{
	size_t slash_count = 0;
	const wchar_t *character = argument;
	if (*length + 2 >= capacity) return;
	command[(*length)++] = L' ';
	command[(*length)++] = L'"';
	while (*character && *length + 4 < capacity) {
		if (*character == L'\\') {
			++slash_count;
			++character;
		} else if (*character == L'"') {
			while (slash_count-- > 0 && *length + 2 < capacity) command[(*length)++] = L'\\';
			slash_count = 0;
			command[(*length)++] = L'\\';
			command[(*length)++] = L'"';
			++character;
		} else {
			while (slash_count-- > 0 && *length + 1 < capacity) command[(*length)++] = L'\\';
			slash_count = 0;
			command[(*length)++] = *character++;
		}
	}
	while (slash_count-- > 0 && *length + 2 < capacity) command[(*length)++] = L'\\';
	if (*length + 1 < capacity) command[(*length)++] = L'"';
	command[*length] = L'\0';
}

static void AppendSwitch(wchar_t *command, size_t capacity, size_t *length,
                         const wchar_t *name, const wchar_t *value)
{
	if (*length + wcslen(name) + 1 >= capacity) return;
	command[(*length)++] = L' ';
	wcscpy_s(command + *length, capacity - *length, name);
	*length += wcslen(name);
	QuoteArgument(command, capacity, length, value);
}

static int RunPowerShell(WorkerJob *job)
{
	wchar_t command[8192] = L"powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File";
	size_t length = wcslen(command);
	SECURITY_ATTRIBUTES security = { sizeof(security), NULL, TRUE };
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	HANDLE read_pipe = NULL;
	HANDLE write_pipe = NULL;
	char output[RESPONSE_TEXT_CAPACITY] = { 0 };
	size_t output_length = 0;
	DWORD read_count;
	DWORD exit_code = 1;
	memset(&startup, 0, sizeof(startup));
	memset(&process, 0, sizeof(process));
	QuoteArgument(command, _countof(command), &length, job->helper);
	if (job->kind == JOB_PING) {
		AppendSwitch(command, _countof(command), &length, L"-Action", L"Ping");
		AppendSwitch(command, _countof(command), &length, L"-ConfigPath", job->config);
	} else if (job->kind == JOB_CREATE) {
		AppendSwitch(command, _countof(command), &length, L"-Action", L"Create");
		AppendSwitch(command, _countof(command), &length, L"-InputPath", job->input);
		AppendSwitch(command, _countof(command), &length, L"-OutputPath", job->output);
		AppendSwitch(command, _countof(command), &length, L"-ConfigPath", job->config);
	} else if (job->kind == JOB_FETCH) {
		wchar_t count[16];
		AppendSwitch(command, _countof(command), &length, L"-Action", L"Fetch");
		AppendSwitch(command, _countof(command), &length, L"-OutputPath", job->output);
		AppendSwitch(command, _countof(command), &length, L"-ConfigPath", job->config);
		AppendSwitch(command, _countof(command), &length, L"-FormId", job->form_id);
		_snwprintf_s(count, _countof(count), _TRUNCATE, L"%d", job->expected_responses);
		AppendSwitch(command, _countof(command), &length, L"-ExpectedResponses", count);
	} else {
		AppendSwitch(command, _countof(command), &length, L"-Action", L"Cancel");
		AppendSwitch(command, _countof(command), &length, L"-ConfigPath", job->config);
		AppendSwitch(command, _countof(command), &length, L"-FormId", job->form_id);
	}
	if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) {
		_snwprintf_s(job->message, _countof(job->message), _TRUNCATE,
			L"Could not create process output pipe (%lu).", GetLastError());
		return 0;
	}
	SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	startup.hStdOutput = write_pipe;
	startup.hStdError = write_pipe;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	if (!CreateProcessW(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
	                    NULL, NULL, &startup, &process)) {
		_snwprintf_s(job->message, _countof(job->message), _TRUNCATE,
			L"Could not start PowerShell (%lu).", GetLastError());
		CloseHandle(read_pipe);
		CloseHandle(write_pipe);
		return 0;
	}
	CloseHandle(write_pipe);
	while (ReadFile(read_pipe, output + output_length,
	                (DWORD)(sizeof(output) - output_length - 1), &read_count, NULL) && read_count) {
		output_length += read_count;
		if (output_length + 1 >= sizeof(output)) break;
	}
	output[output_length] = '\0';
	CloseHandle(read_pipe);
	WaitForSingleObject(process.hProcess, 60000);
	if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
		TerminateProcess(process.hProcess, 1);
		WaitForSingleObject(process.hProcess, 5000);
		_snwprintf_s(job->message, _countof(job->message), _TRUNCATE,
			L"The Apps Script request timed out.");
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return 0;
	}
	GetExitCodeProcess(process.hProcess, &exit_code);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	job->exit_code = (int)exit_code;
	if (exit_code != 0) {
		UINT code_page = IsValidCodePage(CP_OEMCP) ? CP_OEMCP : CP_UTF8;
		if (!MultiByteToWideChar(code_page, 0, output, -1,
		                         job->message, (int)_countof(job->message))) {
			CopyWide(job->message, _countof(job->message), L"Apps Script request failed.");
		}
		return 0;
	}
	return 1;
}

static unsigned __stdcall WorkerMain(void *parameter)
{
	WorkerJob *job = (WorkerJob *)parameter;
	int success = RunPowerShell(job);
	if (!PostMessageW(job->window, WM_APP_JOB_DONE, (WPARAM)success, (LPARAM)job)) free(job);
	return 0;
}

static int StartWorker(AppState *app, JobKind kind, const GroupEvent *event)
{
	WorkerJob *job;
	uintptr_t thread;
	if (app->busy) return 0;
	job = (WorkerJob *)calloc(1, sizeof(*job));
	if (job == NULL) return 0;
	job->window = app->window;
	job->kind = kind;
	CopyWide(job->helper, _countof(job->helper), app->helper_path);
	CopyWide(job->config, _countof(job->config), app->config_path);
	if (event != NULL) {
		CopyWide(job->input, _countof(job->input), event->directory);
		JoinPath(job->input, _countof(job->input), event->directory, L"event.json");
		JoinPath(job->output, _countof(job->output), event->directory,
			kind == JOB_CREATE ? L"form-details.txt" : L"responses.tsv");
		CopyWide(job->form_id, _countof(job->form_id), L"");
		if (kind == JOB_FETCH || kind == JOB_CANCEL) {
			Utf8ToWide(event->form_id, job->form_id, _countof(job->form_id));
		}
		if (kind == JOB_FETCH) {
			job->expected_responses = event->expected_responses;
		}
	}
	thread = _beginthreadex(NULL, 0, WorkerMain, job, 0, NULL);
	if (thread == 0) {
		free(job);
		return 0;
	}
	CloseHandle((HANDLE)thread);
	app->busy = 1;
	EnableWindow(app->create_button, FALSE);
	EnableWindow(app->refresh_button, FALSE);
	EnableWindow(app->cancel_event_button, FALSE);
	EnableWindow(app->test_connection_button, FALSE);
	return 1;
}

static void RemoveFailedEvent(const wchar_t *directory)
{
	wchar_t path[MAX_PATH];
	JoinPath(path, MAX_PATH, directory, L"event.json"); DeleteFileW(path);
	JoinPath(path, MAX_PATH, directory, L"form-details.txt"); DeleteFileW(path);
	JoinPath(path, MAX_PATH, directory, L"responses.tsv"); DeleteFileW(path);
	RemoveDirectoryW(directory);
}

static int GetControlTextUtf8(HWND control, char *output, size_t capacity)
{
	int length = GetWindowTextLengthW(control);
	wchar_t *wide;
	int success;
	if (length < 0 || length > 4096) return 0;
	wide = (wchar_t *)calloc((size_t)length + 1, sizeof(wchar_t));
	if (wide == NULL) return 0;
	GetWindowTextW(control, wide, length + 1);
	success = WideToUtf8(wide, output, capacity);
	free(wide);
	return success;
}

static void DefaultTimeZone(wchar_t *output, size_t capacity)
{
	DYNAMIC_TIME_ZONE_INFORMATION time_zone;
	if (GetDynamicTimeZoneInformation(&time_zone) != TIME_ZONE_ID_INVALID &&
	    time_zone.StandardName[0]) {
		CopyWide(output, capacity, time_zone.StandardName);
	} else {
		CopyWide(output, capacity, L"Organizer time zone");
	}
}

static int PrepareEvent(AppState *app, GroupEvent *event)
{
	wchar_t text[256];
	int index;
	SYSTEMTIME now;
	memset(event, 0, sizeof(*event));
	if (!GetControlTextUtf8(app->create_name, event->event_name, sizeof(event->event_name)) ||
	    event->event_name[0] == '\0') {
		MessageBoxW(app->window, L"Enter an event name of 1 to 99 UTF-8 bytes.",
			L"GroupSync", MB_OK | MB_ICONWARNING);
		SetFocus(app->create_name);
		return 0;
	}
	if (!ReadIntControl(app->create_duration, 1, 1440, &event->duration_minutes)) {
		MessageBoxW(app->window, L"Meeting duration must be between 1 and 1440 minutes.",
			L"GroupSync", MB_OK | MB_ICONWARNING);
		SetFocus(app->create_duration);
		return 0;
	}
	if (!ReadIntControl(app->create_expected, 1, MAX_PARTICIPANTS, &event->expected_responses)) {
		MessageBoxW(app->window, L"Expected participants must be between 1 and 50.",
			L"GroupSync", MB_OK | MB_ICONWARNING);
		SetFocus(app->create_expected);
		return 0;
	}
	if (!GetControlTextUtf8(app->create_timezone, event->time_zone, sizeof(event->time_zone)) ||
	    event->time_zone[0] == '\0') {
		MessageBoxW(app->window, L"Enter the organizer's time-zone label.",
			L"GroupSync", MB_OK | MB_ICONWARNING);
		SetFocus(app->create_timezone);
		return 0;
	}
	event->date_count = app->date_row_count;
	if (event->date_count < 1 || event->date_count > MAX_DATES) return 0;
	for (index = 0; index < event->date_count; ++index) {
		wchar_t date_text[64];
		wchar_t start_text[32];
		wchar_t end_text[32];
		int start_minutes;
		int end_minutes;
		GetWindowTextW(app->date_controls[index].date, date_text, _countof(date_text));
		GetWindowTextW(app->date_controls[index].start, start_text, _countof(start_text));
		GetWindowTextW(app->date_controls[index].end, end_text, _countof(end_text));
		if (!ParseDateText(date_text) || !ParseTimeText(start_text, 0, &start_minutes) ||
		    !ParseTimeText(end_text, 1, &end_minutes) || end_minutes <= start_minutes) {
			_snwprintf_s(text, _countof(text), _TRUNCATE,
				L"Check date %d. Use YYYY-MM-DD and 24-hour HH:MM times on 30-minute boundaries.",
				index + 1);
			MessageBoxW(app->window, text, L"GroupSync", MB_OK | MB_ICONWARNING);
			SetFocus(app->date_controls[index].date);
			return 0;
		}
		if (wcslen(date_text) >= sizeof(event->dates[index].label)) return 0;
		WideToUtf8(date_text, event->dates[index].label, sizeof(event->dates[index].label));
		event->dates[index].start_minutes = start_minutes;
		event->dates[index].end_minutes = end_minutes;
		event->dates[index].block_count = (end_minutes - start_minutes) / 30;
	}
	(void)now;
	return 1;
}

static int CreateEventDirectory(AppState *app, GroupEvent *event)
{
	SYSTEMTIME now;
	wchar_t event_id[64];
	int attempt;
	GetLocalTime(&now);
	for (attempt = 0; attempt < 100; ++attempt) {
		_snwprintf_s(event_id, _countof(event_id), _TRUNCATE,
			L"%04u%02u%02u-%02u%02u%02u-%03u-%02d",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
			now.wSecond, now.wMilliseconds, attempt);
		JoinPath(event->directory, _countof(event->directory), app->events_dir, event_id);
		if (CreateDirectoryW(event->directory, NULL)) return 1;
		if (GetLastError() != ERROR_ALREADY_EXISTS) return 0;
	}
	return 0;
}

static int BeginCreateEvent(AppState *app)
{
	wchar_t path[MAX_PATH];
	GroupEvent event;
	if (!app->configured) {
		MessageBoxW(app->window, L"Save Apps Script settings before creating an event.",
			L"GroupSync", MB_OK | MB_ICONINFORMATION);
		ShowPage(app, PAGE_SETTINGS);
		return 0;
	}
	if (!PrepareEvent(app, &event)) return 0;
	if (!CreateEventDirectory(app, &event)) {
		MessageBoxW(app->window, L"Could not create the local event folder.",
			L"GroupSync", MB_OK | MB_ICONERROR);
		return 0;
	}
	JoinPath(path, _countof(path), event.directory, L"event.json");
	if (!WriteEvent(&event, path)) {
		RemoveFailedEvent(event.directory);
		MessageBoxW(app->window, L"Could not save the event details.",
			L"GroupSync", MB_OK | MB_ICONERROR);
		return 0;
	}
	app->pending = event;
	SetStatus(app->create_status, L"Creating the Google Form...");
	if (!StartWorker(app, JOB_CREATE, &app->pending)) {
		RemoveFailedEvent(event.directory);
		SetStatus(app->create_status, L"Could not start the background request.");
		return 0;
	}
	return 1;
}

static int SaveCurrentSettings(AppState *app)
{
	wchar_t endpoint[1024];
	wchar_t api_key[1024];
	const wchar_t prefix[] = L"https://script.google.com/macros/s/";
	size_t endpoint_length;
	size_t prefix_length = _countof(prefix) - 1;
	const wchar_t *script_id;
	const wchar_t *suffix;
	GetWindowTextW(app->settings_endpoint, endpoint, _countof(endpoint));
	GetWindowTextW(app->settings_key, api_key, _countof(api_key));
	endpoint_length = wcslen(endpoint);
	if (wcsncmp(endpoint, prefix, prefix_length) != 0 ||
	    endpoint_length <= prefix_length + 5 || endpoint_length > 900 ||
	    wcsncmp(endpoint + endpoint_length - 5, L"/exec", 5) != 0) {
		SetStatus(app->settings_status, L"Enter the deployed Apps Script URL ending in /exec.");
		SetFocus(app->settings_endpoint);
		return 0;
	}
	script_id = endpoint + prefix_length;
	suffix = endpoint + endpoint_length - 5;
	{
		const wchar_t *character;
		for (character = script_id; character < suffix; ++character) {
			if (*character == L'/' || iswspace(*character)) {
				SetStatus(app->settings_status, L"Enter the deployed Apps Script URL ending in /exec.");
				SetFocus(app->settings_endpoint);
				return 0;
			}
		}
	}
	if (api_key[0] == L'\0') {
		SetStatus(app->settings_status, L"Enter the Apps Script API key.");
		SetFocus(app->settings_key);
		return 0;
	}
	if (!SaveConfig(app->config_path, endpoint, api_key)) {
		SetStatus(app->settings_status, L"Could not save settings in your Windows user profile.");
		return 0;
	}
	app->configured = 1;
	SecureZeroMemory(api_key, sizeof(api_key));
	SetStatus(app->settings_status, L"Settings saved. Test the connection to verify them.");
	return 1;
}

static void OpenUrl(const char *url)
{
	wchar_t wide_url[2048];
	if (url[0] && Utf8ToWide(url, wide_url, _countof(wide_url)))
		ShellExecuteW(NULL, L"open", wide_url, NULL, NULL, SW_SHOWNORMAL);
}

static void CopyResponderLink(AppState *app)
{
	wchar_t text[2048];
	HGLOBAL memory;
	wchar_t *target;
	if (!app->current.responder_url[0] ||
	    !Utf8ToWide(app->current.responder_url, text, _countof(text))) return;
	if (!OpenClipboard(app->window)) return;
	EmptyClipboard();
	memory = GlobalAlloc(GMEM_MOVEABLE, (wcslen(text) + 1) * sizeof(wchar_t));
	if (memory) {
		target = (wchar_t *)GlobalLock(memory);
		if (target) {
			wcscpy_s(target, wcslen(text) + 1, text);
			GlobalUnlock(memory);
			if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
		} else GlobalFree(memory);
	}
	CloseClipboard();
	SetStatus(app->dashboard_status, L"Responder link copied to the clipboard.");
}

static void RefreshEventList(AppState *app)
{
	WIN32_FIND_DATAW data;
	HANDLE search;
	wchar_t pattern[MAX_PATH];
	int selected = -1;
	int index;
	app->saved_event_count = 0;
	SendMessageW(app->event_list, LB_RESETCONTENT, 0, 0);
	_snwprintf_s(pattern, _countof(pattern), _TRUNCATE, L"%ls\\*", app->events_dir);
	search = FindFirstFileW(pattern, &data);
	if (search == INVALID_HANDLE_VALUE) {
		SetStatus(app->events_hint, L"No saved events yet.");
		return;
	}
	do {
		wchar_t directory[MAX_PATH];
		wchar_t event_path[MAX_PATH];
		wchar_t cancelled_path[MAX_PATH];
		GroupEvent event;
		int row;
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
		    data.cFileName[0] == L'.' || app->saved_event_count >= MAX_EVENTS) continue;
		JoinPath(directory, _countof(directory), app->events_dir, data.cFileName);
		JoinPath(event_path, _countof(event_path), directory, L"event.json");
		JoinPath(cancelled_path, _countof(cancelled_path), directory, L"cancelled.flag");
		if (GetFileAttributesW(cancelled_path) != INVALID_FILE_ATTRIBUTES) continue;
		if (!LoadEventDirectory(directory, &event)) continue;
		CopyWide(app->saved_events[app->saved_event_count].directory,
			_countof(app->saved_events[app->saved_event_count].directory), directory);
		Utf8ToWide(event.event_name,
			app->saved_events[app->saved_event_count].display_name,
			_countof(app->saved_events[app->saved_event_count].display_name));
		row = (int)SendMessageW(app->event_list, LB_ADDSTRING, 0,
			(LPARAM)app->saved_events[app->saved_event_count].display_name);
		SendMessageW(app->event_list, LB_SETITEMDATA, row,
			(LPARAM)app->saved_event_count);
		if (app->has_event && _wcsicmp(app->current.directory, directory) == 0) selected = row;
		++app->saved_event_count;
	} while (FindNextFileW(search, &data));
	FindClose(search);
	if (selected >= 0) SendMessageW(app->event_list, LB_SETCURSEL, selected, 0);
	SetStatus(app->events_hint, app->saved_event_count ?
		L"Select an event to open its dashboard." : L"No saved events yet.");
	for (index = app->saved_event_count; index < MAX_EVENTS; ++index)
		app->saved_events[index].directory[0] = L'\0';
}

static int LoadResponses(GroupEvent *event)
{
	wchar_t path[MAX_PATH];
	char *tsv = NULL;
	JoinPath(path, _countof(path), event->directory, L"responses.tsv");
	if (!ReadWholeFile(path, &tsv, NULL)) {
		event->participant_count = 0;
		event->recommendation_count = 0;
		return 0;
	}
	ParseResponses(tsv, event);
	free(tsv);
	return event->participant_count;
}

static void SelectSavedEvent(AppState *app, int row)
{
	LRESULT index;
	GroupEvent event;
	if (row < 0) return;
	index = SendMessageW(app->event_list, LB_GETITEMDATA, row, 0);
	if (index < 0 || index >= app->saved_event_count) return;
	if (!LoadEventDirectory(app->saved_events[index].directory, &event)) {
		SetStatus(app->events_hint, L"Could not read this event's local files.");
		return;
	}
	app->current = event;
	app->has_event = 1;
	UpdateDashboard(app);
	ShowPage(app, PAGE_DASHBOARD);
}

static int MarkEventCancelled(const GroupEvent *event)
{
	wchar_t marker_path[MAX_PATH];
	JoinPath(marker_path, _countof(marker_path), event->directory, L"cancelled.flag");
	return WriteUtf8File(marker_path, "Cancelled remotely.\r\n");
}

static void CancelCurrentEvent(AppState *app)
{
	wchar_t event_name[MAX_EVENT_NAME];
	wchar_t confirmation[768];
	if (!app->has_event || !app->current.form_id[0] || app->busy) return;
	if (!Utf8ToWide(app->current.event_name, event_name, _countof(event_name)))
		CopyWide(event_name, _countof(event_name), L"this event");
	_snwprintf_s(confirmation, _countof(confirmation), _TRUNCATE,
		L"Cancel \"%ls\"? The Google Form will stop accepting responses, and existing responses will be preserved. The event will be removed from My Events on this computer, but its files will be kept.\r\n\r\nThis action cannot be undone from GroupSync.",
		event_name);
	if (MessageBoxW(app->window, confirmation, L"Cancel Event",
		MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
	SetStatus(app->dashboard_status, L"Canceling the Google Form and preserving responses...");
	if (!StartWorker(app, JOB_CANCEL, &app->current)) {
		SetStatus(app->dashboard_status, L"Could not start the cancellation request.");
		return;
	}
}

static void StartFetch(AppState *app)
{
	if (!app->has_event || !app->current.form_id[0] || app->busy) return;
	SetStatus(app->dashboard_status, L"Refreshing responses...");
	if (!StartWorker(app, JOB_FETCH, &app->current))
		SetStatus(app->dashboard_status, L"Could not start the background request.");
}

static void CompleteJob(AppState *app, WorkerJob *job, int success)
{
	app->busy = 0;
	EnableWindow(app->create_button, TRUE);
	EnableWindow(app->refresh_button, app->has_event && app->current.form_id[0] != '\0');
	EnableWindow(app->cancel_event_button,
		app->has_event && app->current.form_id[0] != '\0');
	EnableWindow(app->test_connection_button, TRUE);
	if (job->kind == JOB_PING) {
		if (success) {
			UpdateConnection(app, 1, L"Connection test succeeded.");
		} else {
			UpdateConnection(app, -1, job->message[0] ? job->message : L"Connection test failed.");
		}
	} else if (job->kind == JOB_CREATE) {
		if (success) {
			GroupEvent created = app->pending;
			ReadFormDetails(&created);
			if (!created.form_id[0] || !created.responder_url[0]) {
				RemoveFailedEvent(created.directory);
				SetStatus(app->create_status, L"The service did not return a form link.");
			} else {
				app->current = created;
				app->has_event = 1;
				UpdateConnection(app, 1, NULL);
				UpdateDashboard(app);
				RefreshEventList(app);
				ShowPage(app, PAGE_DASHBOARD);
				SetStatus(app->dashboard_status, L"Event created. Share the responder link with your group.");
			}
		} else {
			RemoveFailedEvent(app->pending.directory);
			SetStatus(app->create_status, job->message[0] ? job->message : L"Could not create the event.");
		}
	} else if (job->kind == JOB_CANCEL) {
		if (success) {
			if (MarkEventCancelled(&app->current)) {
				app->has_event = 0;
				SecureZeroMemory(&app->current, sizeof(app->current));
				UpdateDashboard(app);
				RefreshEventList(app);
				SetStatus(app->dashboard_status,
					L"Event canceled. New responses are closed; existing responses were preserved.");
			} else {
				SetStatus(app->dashboard_status,
					L"Google Form is canceled, but this event could not be archived locally. Retry Cancel Event.");
			}
		} else {
			wchar_t error[1200];
			_snwprintf_s(error, _countof(error), _TRUNCATE,
				L"Google Form cancellation failed. The event is still active and can be retried.\r\n%ls",
				job->message[0] ? job->message : L"Unknown Apps Script error.");
			SetStatus(app->dashboard_status, error);
			MessageBoxW(app->window, error, L"Cancel Event Failed",
				MB_OK | MB_ICONERROR);
		}
	} else {
		if (success) {
			LoadResponses(&app->current);
			UpdateDashboard(app);
			SetStatus(app->dashboard_status, L"Responses refreshed.");
		} else {
			SetStatus(app->dashboard_status, job->message[0] ? job->message : L"Could not refresh responses.");
		}
	}
	free(job);
}

static void AddDateRow(AppState *app)
{
	SYSTEMTIME now;
	wchar_t date[32];
	int index = app->date_row_count;
	if (index >= MAX_DATES) return;
	GetLocalTime(&now);
	_snwprintf_s(date, _countof(date), _TRUNCATE, L"%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);
	SetWindowTextW(app->date_controls[index].date, date);
	++app->date_row_count;
	LayoutApp(app);
}

static void RemoveDateRow(AppState *app, int index)
{
	int row;
	if (index < 0 || index >= app->date_row_count || app->date_row_count <= 1) return;
	for (row = index; row + 1 < app->date_row_count; ++row) {
		wchar_t text[64];
		GetWindowTextW(app->date_controls[row + 1].date, text, _countof(text));
		SetWindowTextW(app->date_controls[row].date, text);
		GetWindowTextW(app->date_controls[row + 1].start, text, _countof(text));
		SetWindowTextW(app->date_controls[row].start, text);
		GetWindowTextW(app->date_controls[row + 1].end, text, _countof(text));
		SetWindowTextW(app->date_controls[row].end, text);
	}
	--app->date_row_count;
	SetWindowTextW(app->date_controls[app->date_row_count].date, L"");
	SetWindowTextW(app->date_controls[app->date_row_count].start, L"09:00");
	SetWindowTextW(app->date_controls[app->date_row_count].end, L"17:00");
	LayoutApp(app);
}

static void SaveAndTestSettings(AppState *app)
{
	if (!SaveCurrentSettings(app)) return;
	if (!StartWorker(app, JOB_PING, NULL)) {
		SetStatus(app->settings_status, L"Could not start the connection test.");
		return;
	}
	SetStatus(app->settings_status, L"Testing Apps Script connection...");
	UpdateConnection(app, 0, NULL);
}

static void DrawAvailabilityGrid(AppState *app, HWND window, HDC dc,
                                 const RECT *client)
{
	int row_header = Scale(app, 92);
	int column_width = Scale(app, 116);
	int header_height = Scale(app, 36);
	int row_height = Scale(app, 34);
	int minimum = 0;
	int maximum = 0;
	int row_count;
	int row;
	int date_index;
	int earliest = 1440;
	int latest = 0;
	HBRUSH background = CreateSolidBrush(GS_COLOR_SURFACE);
	RECT fill = *client;
	FillRect(dc, &fill, background);
	DeleteObject(background);
	if (!app->has_event || app->current.date_count == 0) {
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, GS_COLOR_TEXT_SECONDARY);
		SelectObject(dc, app->font);
		DrawTextW(dc, L"Create an event to see its availability grid.", -1,
			&fill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}
	for (date_index = 0; date_index < app->current.date_count; ++date_index) {
		if (app->current.dates[date_index].start_minutes < earliest)
			earliest = app->current.dates[date_index].start_minutes;
		if (app->current.dates[date_index].end_minutes > latest)
			latest = app->current.dates[date_index].end_minutes;
	}
	row_count = (latest - earliest) / 30;
	maximum = row_header + app->current.date_count * column_width;
	{
		SCROLLINFO info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		info.nMin = 0;
		info.nMax = maximum;
		info.nPage = (UINT)(client->right - client->left);
		info.nPos = app->grid_scroll_x;
		SetScrollInfo(window, SB_HORZ, &info, TRUE);
		info.nMax = header_height + row_count * row_height;
		info.nPage = (UINT)(client->bottom - client->top);
		info.nPos = app->grid_scroll_y;
		SetScrollInfo(window, SB_VERT, &info, TRUE);
	}
	{
		RECT header = { -app->grid_scroll_x, -app->grid_scroll_y,
			row_header + app->current.date_count * column_width,
			header_height - app->grid_scroll_y };
		HBRUSH header_brush = CreateSolidBrush(GS_COLOR_WINDOW_BG);
		FillRect(dc, &header, header_brush);
		DeleteObject(header_brush);
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, GS_COLOR_TEXT);
		SelectObject(dc, app->font_bold);
		{
			RECT time_header = { header.left, header.top,
				row_header - app->grid_scroll_x, header.bottom };
			DrawTextW(dc, L"Time", -1, &time_header,
				DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
		for (date_index = 0; date_index < app->current.date_count; ++date_index) {
			RECT cell = { row_header + date_index * column_width - app->grid_scroll_x,
				-app->grid_scroll_y,
				row_header + (date_index + 1) * column_width - app->grid_scroll_x,
				header_height - app->grid_scroll_y };
			wchar_t date[64];
			Utf8ToWide(app->current.dates[date_index].label, date, _countof(date));
			DrawTextW(dc, date, -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
		}
	}
	for (row = 0; row < row_count; ++row) {
		int row_minutes = earliest + row * 30;
		RECT time_cell = { -app->grid_scroll_x,
			header_height + row * row_height - app->grid_scroll_y,
			row_header - app->grid_scroll_x,
			header_height + (row + 1) * row_height - app->grid_scroll_y };
		wchar_t time_text[48];
		FormatClock(row_minutes, time_text, _countof(time_text));
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, GS_COLOR_TEXT_SECONDARY);
		SelectObject(dc, app->font);
		DrawTextW(dc, time_text, -1, &time_cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		for (date_index = 0; date_index < app->current.date_count; ++date_index) {
			const DateRange *date = &app->current.dates[date_index];
			RECT cell = { row_header + date_index * column_width - app->grid_scroll_x,
				time_cell.top,
				row_header + (date_index + 1) * column_width - app->grid_scroll_x,
				time_cell.bottom };
			COLORREF color = GS_COLOR_SURFACE;
			int available = 0;
			int block = (row_minutes - date->start_minutes) / 30;
			int in_range = row_minutes >= date->start_minutes && row_minutes + 30 <= date->end_minutes;
			int participant;
			wchar_t count_text[32];
			HBRUSH cell_brush;
			HPEN border_pen;
			HGDIOBJ old_brush;
			HGDIOBJ old_pen;
			int highlighted = 0;
			if (!in_range) {
				wcscpy_s(count_text, _countof(count_text), L"\x2014");
			} else {
				for (participant = 0; participant < app->current.participant_count; ++participant) {
					if (app->current.participants[participant].availability[date_index][block] == AVAILABLE)
						++available;
				}
				_snwprintf_s(count_text, _countof(count_text), _TRUNCATE,
					L"%d/%d", available, app->current.participant_count);
				if (app->current.participant_count > 0) {
					if (available == app->current.participant_count) color = GS_COLOR_AVAILABLE_BG;
					else if (available > 0) color = GS_COLOR_MAYBE_BG;
					else color = GS_COLOR_UNAVAILABLE_BG;
				}
				if (app->current.recommendation_count > 0) {
					Recommendation *best = &app->current.recommendations[0];
					int needed = (app->current.duration_minutes + 29) / 30;
					highlighted = best->date_index == date_index &&
						block >= best->start_block && block < best->start_block + needed;
				}
			}
			cell_brush = CreateSolidBrush(color);
			border_pen = CreatePen(PS_SOLID, 1, GS_COLOR_BORDER);
			old_brush = SelectObject(dc, cell_brush);
			old_pen = SelectObject(dc, border_pen);
			Rectangle(dc, cell.left, cell.top, cell.right, cell.bottom);
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, GS_COLOR_TEXT);
			SelectObject(dc, app->font);
			DrawTextW(dc, count_text, -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			if (highlighted) {
				HPEN highlight_pen = CreatePen(PS_SOLID, Scale(app, 2), GS_COLOR_PRIMARY);
				HGDIOBJ old_highlight = SelectObject(dc, highlight_pen);
				HGDIOBJ old_null = SelectObject(dc, GetStockObject(NULL_BRUSH));
				Rectangle(dc, cell.left + 1, cell.top + 1, cell.right - 1, cell.bottom - 1);
				SelectObject(dc, old_null);
				SelectObject(dc, old_highlight);
				DeleteObject(highlight_pen);
			}
			SelectObject(dc, old_brush);
			SelectObject(dc, old_pen);
			DeleteObject(cell_brush);
			DeleteObject(border_pen);
		}
	}
	(void)minimum;
}

static LRESULT CALLBACK GridProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	AppState *app = (AppState *)GetWindowLongPtrW(window, GWLP_USERDATA);
	if (message == WM_NCCREATE) {
		CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
		SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
		return DefWindowProcW(window, message, wparam, lparam);
	}
	if (app == NULL) return DefWindowProcW(window, message, wparam, lparam);
	switch (message) {
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		RECT client;
		HDC dc = BeginPaint(window, &paint);
		GetClientRect(window, &client);
		DrawAvailabilityGrid(app, window, dc, &client);
		EndPaint(window, &paint);
		return 0;
	}
	case WM_VSCROLL:
	case WM_HSCROLL: {
		int bar = message == WM_VSCROLL ? SB_VERT : SB_HORZ;
		int *position = message == WM_VSCROLL ? &app->grid_scroll_y : &app->grid_scroll_x;
		SCROLLINFO info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(window, bar, &info);
		switch (LOWORD(wparam)) {
		case SB_LINEUP: --*position; break;
		case SB_LINEDOWN: ++*position; break;
		case SB_PAGEUP: *position -= (int)info.nPage; break;
		case SB_PAGEDOWN: *position += (int)info.nPage; break;
		case SB_THUMBTRACK: *position = info.nTrackPos; break;
		case SB_TOP: *position = info.nMin; break;
		case SB_BOTTOM: *position = info.nMax; break;
		default: break;
		}
		if (*position < info.nMin) *position = info.nMin;
		if (*position > info.nMax - (int)info.nPage + 1)
			*position = info.nMax - (int)info.nPage + 1;
		if (*position < 0) *position = 0;
		info.fMask = SIF_POS;
		info.nPos = *position;
		SetScrollInfo(window, bar, &info, TRUE);
		InvalidateRect(window, NULL, TRUE);
		return 0;
	}
	case WM_MOUSEWHEEL:
		SendMessageW(window, WM_VSCROLL,
			(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? SB_LINEUP : SB_LINEDOWN), 0);
		return 0;
	case WM_SIZE:
		InvalidateRect(window, NULL, TRUE);
		return 0;
	}
	return DefWindowProcW(window, message, wparam, lparam);
}

static void UpdateFontsForControlTree(AppState *app)
{
	int page;
	int index;
	for (page = 0; page < PAGE_COUNT; ++page) {
		for (index = 0; index < app->page_control_count[page]; ++index)
			SetControlFont(app->page_controls[page][index], app->font);
	}
	for (page = 0; page < PAGE_COUNT; ++page) SetControlFont(app->nav[page], app->font_bold);
	SetControlFont(app->dashboard_title, app->font_title);
}

static void RebuildFonts(AppState *app)
{
	HFONT old_font = app->font;
	HFONT old_bold = app->font_bold;
	HFONT old_title = app->font_title;
	app->font = CreateFontW(-MulDiv(9, app->dpi ? app->dpi : 96, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	app->font_bold = CreateFontW(-MulDiv(9, app->dpi ? app->dpi : 96, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	app->font_title = CreateFontW(-MulDiv(20, app->dpi ? app->dpi : 96, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	UpdateFontsForControlTree(app);
	SetControlFont(app->connection_label, app->font_bold);
	if (old_font) DeleteObject(old_font);
	if (old_bold) DeleteObject(old_bold);
	if (old_title) DeleteObject(old_title);
}

static void UpdateDateCount(AppState *app)
{
	wchar_t text[64];
	_snwprintf_s(text, _countof(text), _TRUNCATE, L"+  Add date  (%d/%d)",
		app->date_row_count, MAX_DATES);
	SetWindowTextW(app->add_date_button, text);
	EnableWindow(app->add_date_button, app->date_row_count < MAX_DATES);
}

static void LayoutAfterPageChange(AppState *app)
{
	LayoutApp(app);
	UpdateDateCount(app);
}

static void ShowParticipants(AppState *app)
{
	wchar_t *text;
	size_t capacity = 1024 * 1024;
	size_t length = 0;
	int participant_index;
	HWND dialog;
	MSG message;
	if (!app->has_event || app->current.participant_count == 0) return;
	text = (wchar_t *)calloc(capacity, sizeof(wchar_t));
	if (text == NULL) return;
	for (participant_index = 0; participant_index < app->current.participant_count; ++participant_index) {
		Participant *participant = &app->current.participants[participant_index];
		wchar_t name[MAX_PARTICIPANT_NAME * 2];
		wchar_t line[512];
		int date_index;
		if (!Utf8ToWide(participant->name, name, _countof(name))) wcscpy_s(name, _countof(name), L"Participant");
		_snwprintf_s(line, _countof(line), _TRUNCATE, L"%ls\r\n", name);
		if (length + wcslen(line) + 2 < capacity) {
			wcscat_s(text, capacity, line);
			length += wcslen(line);
		}
		for (date_index = 0; date_index < app->current.date_count; ++date_index) {
			DateRange *date = &app->current.dates[date_index];
			wchar_t date_name[64];
			Utf8ToWide(date->label, date_name, _countof(date_name));
			_snwprintf_s(line, _countof(line), _TRUNCATE, L"  %ls: ", date_name);
			if (length + wcslen(line) + 2 < capacity) wcscat_s(text, capacity, line), length += wcslen(line);
			{
				int block;
				for (block = 0; block < date->block_count; ++block) {
					wchar_t time[48];
					const wchar_t *status;
					FormatClock(date->start_minutes + block * 30, time, _countof(time));
					switch (participant->availability[date_index][block]) {
					case AVAILABLE: status = L"Available"; break;
					case MAYBE: status = L"Maybe"; break;
					default: status = L"Unavailable"; break;
					}
					_snwprintf_s(line, _countof(line), _TRUNCATE,
						L"%ls %ls%ls", time, status,
						block + 1 == date->block_count ? L"\r\n" : L"; ");
					if (length + wcslen(line) + 2 >= capacity) break;
					wcscat_s(text, capacity, line);
					length += wcslen(line);
				}
			}
		}
		if (length + 3 < capacity) {
			wcscat_s(text, capacity, L"\r\n");
			length += 2;
		}
	}
	dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
		PARTICIPANTS_CLASS, L"Submitted Participants", WS_POPUP | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, Scale(app, 760), Scale(app, 620),
		app->window, NULL, GetModuleHandleW(NULL), text);
	if (dialog) {
		EnableWindow(app->window, FALSE);
		ShowWindow(dialog, SW_SHOW);
		UpdateWindow(dialog);
		while (IsWindow(dialog) && GetMessageW(&message, NULL, 0, 0) > 0) {
			if (!IsDialogMessageW(dialog, &message)) {
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}
		EnableWindow(app->window, TRUE);
		SetForegroundWindow(app->window);
	}
	free(text);
}

static LRESULT CALLBACK ParticipantsProc(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam)
{
	if (message == WM_CREATE) {
		CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
		HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", (const wchar_t *)create->lpCreateParams,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			12, 12, 720, 520, window, (HMENU)1, GetModuleHandleW(NULL), NULL);
		HWND button = CreateWindowExW(0, L"BUTTON", L"Close",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
			640, 542, 90, 32, window, (HMENU)2, GetModuleHandleW(NULL), NULL);
		SendMessageW(edit, WM_SETFONT, (WPARAM)g_app.font, TRUE);
		SendMessageW(button, WM_SETFONT, (WPARAM)g_app.font_bold, TRUE);
		return 0;
	}
	if (message == WM_SIZE) {
		HWND edit = GetDlgItem(window, 1);
		HWND button = GetDlgItem(window, 2);
		RECT client;
		GetClientRect(window, &client);
		MoveWindow(edit, 12, 12, client.right - 24, client.bottom - 58, TRUE);
		MoveWindow(button, client.right - 102, client.bottom - 42, 90, 30, TRUE);
		return 0;
	}
	if (message == WM_COMMAND && LOWORD(wparam) == 2) {
		DestroyWindow(window);
		return 0;
	}
	if (message == WM_CLOSE) {
		DestroyWindow(window);
		return 0;
	}
	return DefWindowProcW(window, message, wparam, lparam);
}

static void LoadStartupSettings(AppState *app)
{
	wchar_t endpoint[1024] = L"";
	wchar_t api_key[1024] = L"";
	wchar_t project_config[MAX_PATH];
	if (ReadConfig(app->config_path, endpoint, _countof(endpoint), api_key, _countof(api_key))) {
		app->configured = 1;
	} else {
		JoinPath(project_config, _countof(project_config), app->application_dir,
			L"groupsync_apps_script_config.json");
		if (ReadConfig(project_config, endpoint, _countof(endpoint), api_key, _countof(api_key))) {
			app->configured = SaveConfig(app->config_path, endpoint, api_key);
		}
	}
	if (app->configured) {
		SetWindowTextW(app->settings_endpoint, endpoint);
		SetWindowTextW(app->settings_key, api_key);
		SecureZeroMemory(api_key, sizeof(api_key));
	}
}

static LRESULT CALLBACK MainProc(HWND window, UINT message,
                                 WPARAM wparam, LPARAM lparam)
{
	AppState *app = (AppState *)GetWindowLongPtrW(window, GWLP_USERDATA);
	if (message == WM_NCCREATE) {
		CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
		app = (AppState *)create->lpCreateParams;
		app->window = window;
		SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
	}
	if (app == NULL) return DefWindowProcW(window, message, wparam, lparam);
	switch (message) {
	case WM_CREATE:
		app->dpi = 96;
		CreateControls(app);
		LoadStartupSettings(app);
		app->active_page = PAGE_DASHBOARD;
		ShowPage(app, PAGE_DASHBOARD);
		RefreshEventList(app);
		UpdateDashboard(app);
		if (app->configured) {
			UpdateConnection(app, 0, L"Checking Apps Script connection...");
			StartWorker(app, JOB_PING, NULL);
		} else {
			UpdateConnection(app, -2, L"Configure Apps Script to create or refresh events.");
		}
		return 0;
	case WM_SIZE:
		LayoutAfterPageChange(app);
		return 0;
	case WM_VSCROLL:
		if (app->active_page == PAGE_CREATE && app->create_scroll_max > 0) {
			SCROLLINFO info;
			int position = app->create_scroll_y;
			memset(&info, 0, sizeof(info));
			info.cbSize = sizeof(info);
			info.fMask = SIF_ALL;
			GetScrollInfo(window, SB_VERT, &info);
			switch (LOWORD(wparam)) {
			case SB_LINEUP: position -= 30; break;
			case SB_LINEDOWN: position += 30; break;
			case SB_PAGEUP: position -= (int)info.nPage; break;
			case SB_PAGEDOWN: position += (int)info.nPage; break;
			case SB_THUMBTRACK: position = info.nTrackPos; break;
			case SB_TOP: position = 0; break;
			case SB_BOTTOM: position = app->create_scroll_max; break;
			default: return 0;
			}
			position = (position / 30) * 30;
			if (position < 0) position = 0;
			if (position > app->create_scroll_max) position = app->create_scroll_max;
			app->create_scroll_y = position;
			LayoutApp(app);
			return 0;
		}
		break;
	case WM_MOUSEWHEEL:
		if (app->active_page == PAGE_CREATE) {
			SendMessageW(window, WM_VSCROLL,
				(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? SB_LINEUP : SB_LINEDOWN), 0);
			return 0;
		}
		break;
	case WM_DPICHANGED: {
		RECT *suggested = (RECT *)lparam;
		RECT target = *suggested;
		MONITORINFO monitor_info;
		HMONITOR monitor;
		app->dpi = HIWORD(wparam);
		RebuildFonts(app);
		memset(&monitor_info, 0, sizeof(monitor_info));
		monitor_info.cbSize = sizeof(monitor_info);
		monitor = MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST);
		if (GetMonitorInfoW(monitor, &monitor_info)) {
			int width = target.right - target.left;
			int height = target.bottom - target.top;
			int work_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
			int work_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
			if (width > work_width) width = work_width;
			if (height > work_height) height = work_height;
			if (target.left < monitor_info.rcWork.left) target.left = monitor_info.rcWork.left;
			if (target.top < monitor_info.rcWork.top) target.top = monitor_info.rcWork.top;
			if (target.left + width > monitor_info.rcWork.right)
				target.left = monitor_info.rcWork.right - width;
			if (target.top + height > monitor_info.rcWork.bottom)
				target.top = monitor_info.rcWork.bottom - height;
			target.right = target.left + width;
			target.bottom = target.top + height;
		}
		SetWindowPos(window, NULL, target.left, target.top,
			target.right - target.left, target.bottom - target.top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		LayoutAfterPageChange(app);
		return 0;
	}
	case WM_DRAWITEM: {
		DRAWITEMSTRUCT *item = (DRAWITEMSTRUCT *)lparam;
		if (item->CtlType == ODT_BUTTON) {
			DrawButton(app, item);
			return TRUE;
		}
		if (item->CtlType == ODT_STATIC) {
			RECT rect = item->rcItem;
			if (item->CtlID == ID_HEADER) {
				HBRUSH white = CreateSolidBrush(GS_COLOR_SURFACE);
				FillRect(item->hDC, &rect, white);
				DeleteObject(white);
				if (app->logo_bitmap && app->logo_width > 0 && app->logo_height > 0) {
					int logo_draw_width = Scale(app, 150);
					int logo_draw_height = MulDiv(logo_draw_width,
						app->logo_height, app->logo_width);
					HDC logo_dc = CreateCompatibleDC(item->hDC);
					HBITMAP old_logo = (HBITMAP)SelectObject(logo_dc, app->logo_bitmap);
					BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
					AlphaBlend(item->hDC, Scale(app, 20),
						(rect.bottom - logo_draw_height) / 2, logo_draw_width,
						logo_draw_height, logo_dc, 0, 0, app->logo_width,
						app->logo_height, blend);
					SelectObject(logo_dc, old_logo);
					DeleteDC(logo_dc);
				} else {
					DrawLogo(item->hDC, Scale(app, 24), Scale(app, 20), Scale(app, 1));
				}
				SetBkColor(item->hDC, GS_COLOR_BORDER);
				PatBlt(item->hDC, 0, rect.bottom - 1, rect.right, 1, PATCOPY);
				return TRUE;
			}
			if (item->CtlID == ID_SIDEBAR) {
				HBRUSH white = CreateSolidBrush(GS_COLOR_SURFACE);
				HPEN pen = CreatePen(PS_SOLID, 1, GS_COLOR_BORDER);
				HGDIOBJ old_brush = SelectObject(item->hDC, white);
				HGDIOBJ old_pen = SelectObject(item->hDC, pen);
				Rectangle(item->hDC, rect.left, rect.top, rect.right, rect.bottom);
				SelectObject(item->hDC, old_brush);
				SelectObject(item->hDC, old_pen);
				DeleteObject(white);
				DeleteObject(pen);
				SetBkMode(item->hDC, TRANSPARENT);
				SetTextColor(item->hDC, GS_COLOR_TEXT_SECONDARY);
				SelectObject(item->hDC, app->font_bold);
				TextOutW(item->hDC, Scale(app, 20), Scale(app, 12), L"WORKSPACE", 9);
				SetTextColor(item->hDC, app->configured ? GS_COLOR_AVAILABLE : GS_COLOR_TEXT_SECONDARY);
				SelectObject(item->hDC, app->font);
				TextOutW(item->hDC, Scale(app, 20), rect.bottom - Scale(app, 44),
					app->configured ? L"Apps Script configured" : L"Setup required",
					app->configured ? 22 : 14);
				return TRUE;
			}
			if (item->CtlID >= 610 && item->CtlID <= 612) {
				COLORREF fill;
				COLORREF symbol_color;
				const wchar_t *symbol;
				wchar_t text[64];
				RECT swatch = rect;
				if (item->CtlID == 610) {
					fill = GS_COLOR_AVAILABLE_BG;
					symbol_color = GS_COLOR_AVAILABLE;
					symbol = L"\x2713";
				} else if (item->CtlID == 611) {
					fill = GS_COLOR_MAYBE_BG;
					symbol_color = GS_COLOR_MAYBE;
					symbol = L"+";
				} else {
					fill = GS_COLOR_UNAVAILABLE_BG;
					symbol_color = GS_COLOR_UNAVAILABLE;
					symbol = L"x";
				}
				GetWindowTextW(item->hwndItem, text, _countof(text));
				swatch.right = swatch.left + Scale(app, 18);
				swatch.top += (swatch.bottom - swatch.top - Scale(app, 18)) / 2;
				swatch.bottom = swatch.top + Scale(app, 18);
				DrawRoundRect(item->hDC, &swatch, fill, fill, Scale(app, 5));
				SetBkMode(item->hDC, TRANSPARENT);
				SetTextColor(item->hDC, symbol_color);
				SelectObject(item->hDC, app->font_bold);
				DrawTextW(item->hDC, symbol, -1, &swatch, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				SetTextColor(item->hDC, GS_COLOR_TEXT_SECONDARY);
				SelectObject(item->hDC, app->font);
				rect.left += Scale(app, 24);
				DrawTextW(item->hDC, text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
				return TRUE;
			}
			HBRUSH background = CreateSolidBrush(GS_COLOR_SURFACE);
			FillRect(item->hDC, &rect, background);
			DeleteObject(background);
			InflateRect(&rect, -1, -1);
			DrawRoundRect(item->hDC, &rect, GS_COLOR_SURFACE, GS_COLOR_BORDER, Scale(app, 8));
			return TRUE;
		}
		break;
	}
	case WM_COMMAND: {
		int id = LOWORD(wparam);
		if (id == ID_NAV_DASHBOARD) ShowPage(app, PAGE_DASHBOARD);
		else if (id == ID_NAV_CREATE) ShowPage(app, PAGE_CREATE);
		else if (id == ID_NAV_EVENTS) ShowPage(app, PAGE_EVENTS);
		else if (id == ID_NAV_SETTINGS) ShowPage(app, PAGE_SETTINGS);
		else if (id == ID_ADD_DATE) AddDateRow(app);
		else if (id >= ID_REMOVE_DATE_BASE && id < ID_REMOVE_DATE_BASE + MAX_DATES)
			RemoveDateRow(app, id - ID_REMOVE_DATE_BASE);
		else if (id == ID_CREATE_EVENT) BeginCreateEvent(app);
		else if (id == ID_REFRESH) StartFetch(app);
		else if (id == ID_COPY_LINK) CopyResponderLink(app);
		else if (id == ID_OPEN_SHEET) OpenUrl(app->current.sheet_url);
		else if (id == ID_CANCEL_EVENT) CancelCurrentEvent(app);
		else if (id == ID_VIEW_PARTICIPANTS) ShowParticipants(app);
		else if (id == ID_SAVE_SETTINGS) {
			if (SaveCurrentSettings(app)) UpdateConnection(app, -2, L"Settings saved.");
		}
		else if (id == ID_TEST_CONNECTION) SaveAndTestSettings(app);
		else if (id == ID_EVENT_LIST && HIWORD(wparam) == LBN_SELCHANGE)
			SelectSavedEvent(app, (int)SendMessageW(app->event_list, LB_GETCURSEL, 0, 0));
		return 0;
	}
	case WM_APP_JOB_DONE:
		CompleteJob(app, (WorkerJob *)lparam, (int)wparam);
		return 0;
	case WM_GETMINMAXINFO: {
		MINMAXINFO *limits = (MINMAXINFO *)lparam;
		limits->ptMinTrackSize.x = Scale(app, 1200);
		limits->ptMinTrackSize.y = Scale(app, 760);
		return 0;
	}
	case WM_CLOSE:
		if (app->busy) {
			MessageBoxW(window, L"Wait for the background request to finish before closing GroupSync.",
				L"GroupSync", MB_OK | MB_ICONINFORMATION);
			return 0;
		}
		DestroyWindow(window);
		return 0;
	case WM_DESTROY:
		if (app->font) DeleteObject(app->font);
		if (app->font_bold) DeleteObject(app->font_bold);
		if (app->font_title) DeleteObject(app->font_title);
		if (app->logo_bitmap) DeleteObject(app->logo_bitmap);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(window, message, wparam, lparam);
}

static int RegisterWindowClasses(HINSTANCE instance)
{
	WNDCLASSEXW window_class;
	memset(&window_class, 0, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	window_class.hInstance = instance;
	window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
	window_class.hbrBackground = CreateSolidBrush(GS_COLOR_WINDOW_BG);
	window_class.lpfnWndProc = MainProc;
	window_class.lpszClassName = APP_CLASS;
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClassExW(&window_class)) return 0;
	window_class.lpfnWndProc = GridProc;
	window_class.lpszClassName = GRID_CLASS;
	window_class.hbrBackground = NULL;
	if (!RegisterClassExW(&window_class)) return 0;
	window_class.lpfnWndProc = ParticipantsProc;
	window_class.lpszClassName = PARTICIPANTS_CLASS;
	window_class.hbrBackground = CreateSolidBrush(GS_COLOR_SURFACE);
	return RegisterClassExW(&window_class) != 0;
}

static void InitializePaths(AppState *app)
{
	wchar_t executable[MAX_PATH];
	wchar_t local_app_data[MAX_PATH] = L"";
	wchar_t *separator;
	if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL,
		SHGFP_TYPE_CURRENT, local_app_data))) {
		GetCurrentDirectoryW(_countof(local_app_data), local_app_data);
	}
	JoinPath(app->data_dir, _countof(app->data_dir), local_app_data, L"GroupSync");
	EnsureDirectory(app->data_dir);
	JoinPath(app->events_dir, _countof(app->events_dir), app->data_dir, L"events");
	EnsureDirectory(app->events_dir);
	JoinPath(app->config_path, _countof(app->config_path), app->data_dir,
		L"groupsync_apps_script_config.json");
	GetModuleFileNameW(NULL, executable, _countof(executable));
	separator = wcsrchr(executable, L'\\');
	if (separator) *separator = L'\0';
	CopyWide(app->application_dir, _countof(app->application_dir), executable);
	JoinPath(app->helper_path, _countof(app->helper_path), app->application_dir,
		L"groupsync_apps_script.ps1");
}

static HBITMAP LoadLogoBitmap(AppState *app, int *logo_width, int *logo_height)
{
	IWICImagingFactory *factory = NULL;
	IWICBitmapDecoder *decoder = NULL;
	IWICBitmapFrameDecode *frame = NULL;
	IWICFormatConverter *converter = NULL;
	wchar_t path[MAX_PATH];
	UINT width = 0;
	UINT height = 0;
	UINT stride;
	UINT pixel_bytes;
	BYTE *pixels = NULL;
	void *dib_bits = NULL;
	BITMAPINFO bitmap_info;
	HBITMAP bitmap = NULL;
	HRESULT result;
	HRESULT apartment_result;
	int uninitialize_com;

	JoinPath(path, _countof(path), app->application_dir, L"GroupSyncLogo.png");
	apartment_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	uninitialize_com = SUCCEEDED(apartment_result);
	if (FAILED(apartment_result) && apartment_result != RPC_E_CHANGED_MODE) return NULL;
	result = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
		&IID_IWICImagingFactory, (void **)&factory);
	if (FAILED(result)) goto cleanup;
	result = IWICImagingFactory_CreateDecoderFromFilename(factory, path, NULL,
		GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	if (FAILED(result)) goto cleanup;
	result = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
	if (FAILED(result)) goto cleanup;
	result = IWICImagingFactory_CreateFormatConverter(factory, &converter);
	if (FAILED(result)) goto cleanup;
	result = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame,
		&GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0,
		WICBitmapPaletteTypeCustom);
	if (FAILED(result)) goto cleanup;
	result = IWICBitmapSource_GetSize((IWICBitmapSource *)converter, &width, &height);
	if (FAILED(result) || width == 0 || height == 0 || width > 8192 || height > 8192)
		goto cleanup;
	stride = width * 4;
	if (height > UINT_MAX / stride) goto cleanup;
	pixel_bytes = stride * height;
	pixels = (BYTE *)malloc(pixel_bytes);
	if (pixels == NULL) goto cleanup;
	result = IWICBitmapSource_CopyPixels((IWICBitmapSource *)converter, NULL,
		stride, pixel_bytes, pixels);
	if (FAILED(result)) goto cleanup;
	memset(&bitmap_info, 0, sizeof(bitmap_info));
	bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
	bitmap_info.bmiHeader.biWidth = (LONG)width;
	bitmap_info.bmiHeader.biHeight = -(LONG)height;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	bitmap = CreateDIBSection(NULL, &bitmap_info, DIB_RGB_COLORS, &dib_bits, NULL, 0);
	if (bitmap == NULL || dib_bits == NULL) {
		if (bitmap) DeleteObject(bitmap);
		bitmap = NULL;
		goto cleanup;
	}
	memcpy(dib_bits, pixels, pixel_bytes);
	*logo_width = (int)width;
	*logo_height = (int)height;
cleanup:
	free(pixels);
	if (converter) IWICFormatConverter_Release(converter);
	if (frame) IWICBitmapFrameDecode_Release(frame);
	if (decoder) IWICBitmapDecoder_Release(decoder);
	if (factory) IWICImagingFactory_Release(factory);
	if (uninitialize_com) CoUninitialize();
	return bitmap;
}

static void EnableDpiAwareness(void)
{
	typedef BOOL(WINAPI *SetDpiAwarenessContextFn)(HANDLE);
	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	SetDpiAwarenessContextFn set_context = (SetDpiAwarenessContextFn)
		GetProcAddress(user32, "SetProcessDpiAwarenessContext");
	if (set_context) set_context((HANDLE)(LONG_PTR)-4);
	else SetProcessDPIAware();
}

static void FitWindowToWorkArea(HWND window)
{
	MONITORINFO monitor_info;
	RECT bounds;
	HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
	memset(&monitor_info, 0, sizeof(monitor_info));
	monitor_info.cbSize = sizeof(monitor_info);
	if (!GetMonitorInfoW(monitor, &monitor_info) || !GetWindowRect(window, &bounds)) return;
	{
		int max_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
		int max_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
		int width = bounds.right - bounds.left;
		int height = bounds.bottom - bounds.top;
		int left = bounds.left;
		int top = bounds.top;
		if (width > max_width) width = max_width;
		if (height > max_height) height = max_height;
		if (left < monitor_info.rcWork.left) left = monitor_info.rcWork.left;
		if (top < monitor_info.rcWork.top) top = monitor_info.rcWork.top;
		if (left + width > monitor_info.rcWork.right) left = monitor_info.rcWork.right - width;
		if (top + height > monitor_info.rcWork.bottom) top = monitor_info.rcWork.bottom - height;
		SetWindowPos(window, NULL, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show)
{
	INITCOMMONCONTROLSEX common_controls;
	RECT window_rect = { 0, 0, 1400, 900 };
	RECT work_area;
	UINT initial_dpi;
	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL;
	HWND window;
	MSG message;
	(void)previous;
	(void)command_line;
	EnableDpiAwareness();
	initial_dpi = GetDpiForSystem();
	if (initial_dpi == 0) initial_dpi = 96;
	window_rect.right = MulDiv(1400, initial_dpi, 96);
	window_rect.bottom = MulDiv(900, initial_dpi, 96);
	common_controls.dwSize = sizeof(common_controls);
	common_controls.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&common_controls);
	memset(&g_app, 0, sizeof(g_app));
	g_app.dpi = 96;
	InitializePaths(&g_app);
	g_app.logo_bitmap = LoadLogoBitmap(&g_app, &g_app.logo_width, &g_app.logo_height);
	if (!RegisterWindowClasses(instance)) {
		MessageBoxW(NULL, L"Could not register GroupSync window classes.", L"GroupSync", MB_OK | MB_ICONERROR);
		return 1;
	}
	AdjustWindowRectExForDpi(&window_rect, style, FALSE, 0, initial_dpi);
	if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
		int max_width = work_area.right - work_area.left - 32;
		int max_height = work_area.bottom - work_area.top - 32;
		int width = window_rect.right - window_rect.left;
		int height = window_rect.bottom - window_rect.top;
		if (width > max_width) width = max_width;
		if (height > max_height) height = max_height;
		window_rect.right = window_rect.left + width;
		window_rect.bottom = window_rect.top + height;
	}
	window = CreateWindowExW(WS_EX_APPWINDOW, APP_CLASS, L"GroupSync Availability Matcher",
		style, CW_USEDEFAULT, CW_USEDEFAULT,
		window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
		NULL, NULL, instance, &g_app);
	if (!window) {
		MessageBoxW(NULL, L"Could not create the GroupSync window.", L"GroupSync", MB_OK | MB_ICONERROR);
		return 1;
	}
	g_app.dpi = GetDpiForWindow(window);
	RebuildFonts(&g_app);
	LayoutApp(&g_app);
	ShowWindow(window, show);
	FitWindowToWorkArea(window);
	UpdateWindow(window);
	while (GetMessageW(&message, NULL, 0, 0) > 0) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	return (int)message.wParam;
}
