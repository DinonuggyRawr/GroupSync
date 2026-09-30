#include "availabilitymatcher.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RECOMMENDATIONS 3

static int read_line(const char *prompt, char *buffer, size_t buffer_size)
{
	size_t length;

	printf("%s", prompt);
	if (fgets(buffer, (int)buffer_size, stdin) == NULL) {
		return 0;
	}

	length = strcspn(buffer, "\r\n");
	if (buffer[length] == '\0' && length == buffer_size - 1) {
		int character;
		while ((character = getchar()) != '\n' && character != EOF) {
		}
	}
	buffer[length] = '\0';
	return 1;
}

static int read_nonempty_text(const char *prompt, char *buffer, size_t buffer_size)
{
	while (read_line(prompt, buffer, buffer_size)) {
		if (buffer[0] != '\0') {
			return 1;
		}
		printf("Please enter a value.\n");
	}
	return 0;
}

static int read_integer(const char *prompt, int minimum, int maximum, int *value)
{
	char line[64];

	while (read_line(prompt, line, sizeof(line))) {
		char *end;
		long parsed = strtol(line, &end, 10);

		while (isspace((unsigned char)*end)) {
			++end;
		}
		if (end != line && *end == '\0' && parsed >= minimum && parsed <= maximum) {
			*value = (int)parsed;
			return 1;
		}
		printf("Enter a whole number from %d to %d.\n", minimum, maximum);
	}
	return 0;
}

static int parse_time(const char *text, int allow_end_of_day, int *minutes)
{
	int hour;
	int minute;

	if (strlen(text) != 5 || text[2] != ':' ||
		!isdigit((unsigned char)text[0]) || !isdigit((unsigned char)text[1]) ||
		!isdigit((unsigned char)text[3]) || !isdigit((unsigned char)text[4])) {
		return 0;
	}

	hour = (text[0] - '0') * 10 + text[1] - '0';
	minute = (text[3] - '0') * 10 + text[4] - '0';
	if (minute != 0 && minute != 30) {
		return 0;
	}
	if (allow_end_of_day && hour == 24 && minute == 0) {
		*minutes = 24 * 60;
		return 1;
	}
	if (hour < 0 || hour > 23) {
		return 0;
	}

	*minutes = hour * 60 + minute;
	return 1;
}

static int read_time_value(const char *prompt, int allow_end_of_day, int *minutes)
{
	char line[32];

	while (read_line(prompt, line, sizeof(line))) {
		if (parse_time(line, allow_end_of_day, minutes)) {
			return 1;
		}
		printf("Use 24-hour time in HH:MM format, on a 30-minute boundary%s.\n",
			   allow_end_of_day ? " (end time may be 24:00)" : "");
	}
	return 0;
}

static int write_json_string(FILE *file, const char *value)
{
	const unsigned char *character = (const unsigned char *)value;

	if (fputc('"', file) == EOF) {
		return 0;
	}
	while (*character != '\0') {
		switch (*character) {
		case '"':
			fputs("\\\"", file);
			break;
		case '\\':
			fputs("\\\\", file);
			break;
		case '\n':
			fputs("\\n", file);
			break;
		case '\r':
			fputs("\\r", file);
			break;
		case '\t':
			fputs("\\t", file);
			break;
		default:
			if (*character < 0x20) {
				fprintf(file, "\\u%04x", *character);
			} else {
				fputc(*character, file);
			}
			break;
		}
		++character;
	}
	return fputc('"', file) != EOF && !ferror(file);
}

static int write_event_file(const char *event_name, int duration_minutes,
							const EventDate dates[], int date_count)
{
	FILE *file = fopen("groupsync_event.json", "w");
	int date_index;
	int success;

	if (file == NULL) {
		printf("Could not write groupsync_event.json.\n");
		return 0;
	}
	fputs("{\"eventName\":", file);
	success = write_json_string(file, event_name);
	fprintf(file, ",\"durationMinutes\":%d,\"dates\":[", duration_minutes);
	for (date_index = 0; success && date_index < date_count; ++date_index) {
		if (date_index > 0) {
			fputc(',', file);
		}
		fputs("{\"label\":", file);
		success = write_json_string(file, dates[date_index].label);
		fprintf(file, ",\"startMinutes\":%d,\"endMinutes\":%d,\"blockCount\":%d}",
				dates[date_index].start_minutes, dates[date_index].end_minutes,
				dates[date_index].block_count);
	}
	fputs("]}\n", file);
	if (fclose(file) != 0) {
		success = 0;
	}
	return success;
}

static int run_apps_script(const char *arguments)
{
	char command[2048];
	int command_length = snprintf(
		command, sizeof(command),
		"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"groupsync_apps_script.ps1\" %s",
		arguments);

	if (command_length < 0 || (size_t)command_length >= sizeof(command)) {
		printf("Apps Script command is too long.\n");
		return 0;
	}
	return system(command) == 0;
}

static int ensure_apps_script_setup(void)
{
	FILE *config = fopen("groupsync_apps_script_config.json", "r");

	if (config != NULL) {
		fclose(config);
		return 1;
	}
	printf("First-time Apps Script setup is required.\n");
	return run_apps_script("-Action Setup -ConfigPath \"groupsync_apps_script_config.json\"");
}

static int read_form_details(char *form_id, size_t form_id_size,
							 char *responder_url, size_t responder_url_size)
{
	FILE *file = fopen("groupsync_form.txt", "r");
	size_t length;

	if (file == NULL || fgets(form_id, (int)form_id_size, file) == NULL ||
		fgets(responder_url, (int)responder_url_size, file) == NULL) {
		if (file != NULL) {
			fclose(file);
		}
		printf("Could not read the Google Form details.\n");
		return 0;
	}
	fclose(file);
	length = strcspn(form_id, "\r\n");
	form_id[length] = '\0';
	length = strcspn(responder_url, "\r\n");
	responder_url[length] = '\0';
	return form_id[0] != '\0' && responder_url[0] != '\0';
}

static int read_form_responses(const EventDate dates[], int date_count,
							   Participant participants[], int expected_count,
							   int *participant_count)
{
	FILE *file = fopen("groupsync_responses.tsv", "r");
	char line[4096];

	if (file == NULL) {
		printf("Could not open groupsync_responses.tsv.\n");
		return 0;
	}
	*participant_count = 0;
	while (*participant_count < expected_count &&
		   fgets(line, sizeof(line), file) != NULL) {
		Participant *participant = &participants[*participant_count];
		char *name = strtok(line, "\t\r\n");
		int date_index;

		if (name == NULL) {
			continue;
		}
		snprintf(participant->name, sizeof(participant->name), "%s", name);
		for (date_index = 0; date_index < date_count; ++date_index) {
			int block;
			for (block = 0; block < dates[date_index].block_count; ++block) {
				char *status = strtok(NULL, "\t\r\n");
				char value = status == NULL ? 'U' :
					(char)toupper((unsigned char)status[0]);

				switch (value) {
				case 'A':
					participant->availability[date_index][block] = AVAILABLE;
					break;
				case 'M':
					participant->availability[date_index][block] = MAYBE;
					break;
				default:
					participant->availability[date_index][block] = UNAVAILABLE;
					break;
				}
			}
		}
		++*participant_count;
	}
	fclose(file);
	return *participant_count == expected_count;
}

int main(void)
{
	char event_name[MAX_EVENT_NAME];
	EventDate dates[MAX_DATES];
	Participant participants[MAX_PARTICIPANTS];
	Recommendation recommendations[MAX_RECOMMENDATIONS];
	int duration_minutes;
	int date_count;
	int participant_count;
	int date_index;
	char form_id[256];
	char responder_url[1024];
	char command_arguments[512];
	int character_index;
	int command_length;
	int recommendation_count;

	printf("GroupSync - Group Availability Matcher\n\n");
	if (!read_nonempty_text("Event name: ", event_name, sizeof(event_name)) ||
		!read_integer("Meeting duration in minutes (1-1440): ", 1, 1440,
					  &duration_minutes) ||
		!read_integer("Number of possible dates (1-14): ", 1, MAX_DATES,
					  &date_count)) {
		return 0;
	}

	for (date_index = 0; date_index < date_count; ++date_index) {
		EventDate *date = &dates[date_index];

		printf("\nDate %d\n", date_index + 1);
		if (!read_nonempty_text("  Date label (for example, 2026-10-04): ",
								date->label, sizeof(date->label)) ||
			!read_time_value("  Range start (HH:MM): ", 0,
							 &date->start_minutes) ||
			!read_time_value("  Range end (HH:MM): ", 1,
							 &date->end_minutes)) {
			return 0;
		}
		while (date->end_minutes <= date->start_minutes) {
			printf("The end must be later than the start.\n");
			if (!read_time_value("  Range end (HH:MM): ", 1,
								 &date->end_minutes)) {
				return 0;
			}
		}
		date->block_count =
			(date->end_minutes - date->start_minutes) / 30;
	}

	if (!ensure_apps_script_setup()) {
		printf("Apps Script setup failed. See README.md for setup requirements.\n");
		return 1;
	}
	if (!write_event_file(event_name, duration_minutes, dates, date_count)) {
		return 1;
	}
	if (!run_apps_script("-Action Create -InputPath \"groupsync_event.json\" -OutputPath \"groupsync_form.txt\" -ConfigPath \"groupsync_apps_script_config.json\"") ||
		!read_form_details(form_id, sizeof(form_id), responder_url,
						   sizeof(responder_url))) {
		printf("Could not create the Google Form. See README.md for troubleshooting.\n");
		return 1;
	}
	for (character_index = 0; form_id[character_index] != '\0'; ++character_index) {
		if (!isalnum((unsigned char)form_id[character_index]) &&
			form_id[character_index] != '-' && form_id[character_index] != '_') {
			printf("Google returned an invalid Form ID.\n");
			return 1;
		}
	}

	printf("\nShare this Google Form link with participants:\n%s\n", responder_url);
	if (!read_integer("\nHow many responses should GroupSync wait for (1-50)? ",
					  1, MAX_PARTICIPANTS, &participant_count)) {
		return 0;
	}
	command_length = snprintf(
		command_arguments, sizeof(command_arguments),
		"-Action Collect -InputPath \"groupsync_event.json\" -OutputPath \"groupsync_responses.tsv\" -ConfigPath \"groupsync_apps_script_config.json\" -FormId \"%s\" -ExpectedResponses %d",
		form_id, participant_count);
	if (command_length < 0 || (size_t)command_length >= sizeof(command_arguments)) {
		printf("Google Forms command is too long.\n");
		return 1;
	}
	if (!run_apps_script(command_arguments) ||
		!read_form_responses(dates, date_count, participants, participant_count,
						 &participant_count)) {
		printf("Could not load the requested form responses.\n");
		return 1;
	}

	recommendation_count = find_recommendations(
		dates, date_count, participants, participant_count, duration_minutes,
		recommendations, MAX_RECOMMENDATIONS);
	print_recommendations(event_name, dates, date_count, participants,
						  participant_count, duration_minutes, recommendations,
						  recommendation_count);
	return 0;
}
