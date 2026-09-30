#ifndef AVAILABILITYMATCHER_H
#define AVAILABILITYMATCHER_H

#define MAX_EVENT_NAME 100
#define MAX_DATE_LABEL 32
#define MAX_PARTICIPANTS 50
#define MAX_PARTICIPANT_NAME 64
#define MAX_DATES 14
#define MAX_BLOCKS_PER_DATE 48

typedef enum {
	UNAVAILABLE = 0,
	MAYBE = 1,
	AVAILABLE = 2
} Availability;

typedef struct {
	char label[MAX_DATE_LABEL];
	int start_minutes;
	int end_minutes;
	int block_count;
} EventDate;

typedef struct {
	char name[MAX_PARTICIPANT_NAME];
	Availability availability[MAX_DATES][MAX_BLOCKS_PER_DATE];
} Participant;

typedef struct {
	int date_index;
	int start_block;
	int available_count;
	int maybe_count;
} Recommendation;

int find_recommendations(const EventDate dates[], int date_count,
						 const Participant participants[], int participant_count,
						 int duration_minutes, Recommendation recommendations[],
						 int recommendation_limit);

void print_recommendations(const char *event_name, const EventDate dates[],
						   int date_count, const Participant participants[],
						   int participant_count, int duration_minutes,
						   const Recommendation recommendations[],
						   int recommendation_count);

#endif
