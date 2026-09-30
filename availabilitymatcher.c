#include "availabilitymatcher.h"

#include <stdio.h>

static int participant_status(const Participant *participant, int date_index,
							  int start_block, int blocks_needed)
{
	int has_maybe = 0;
	int block;

	for (block = start_block; block < start_block + blocks_needed; ++block) {
		Availability status = participant->availability[date_index][block];

		if (status == UNAVAILABLE) {
			return UNAVAILABLE;
		}
		if (status == MAYBE) {
			has_maybe = 1;
		}
	}

	return has_maybe ? MAYBE : AVAILABLE;
}

static int ranks_ahead(const Recommendation *left, const Recommendation *right)
{
	if (left->available_count != right->available_count) {
		return left->available_count > right->available_count;
	}
	if (left->maybe_count != right->maybe_count) {
		return left->maybe_count > right->maybe_count;
	}
	if (left->date_index != right->date_index) {
		return left->date_index < right->date_index;
	}
	return left->start_block < right->start_block;
}

int find_recommendations(const EventDate dates[], int date_count,
						 const Participant participants[], int participant_count,
						 int duration_minutes, Recommendation recommendations[],
						 int recommendation_limit)
{
	int blocks_needed = (duration_minutes + 29) / 30;
	int recommendation_count = 0;
	int date_index;

	if (duration_minutes <= 0 || recommendation_limit <= 0) {
		return 0;
	}

	for (date_index = 0; date_index < date_count; ++date_index) {
		int start_block;

		for (start_block = 0;
			 start_block + blocks_needed <= dates[date_index].block_count;
			 ++start_block) {
			Recommendation candidate;
			int participant_index;
			int insert_at;

			candidate.date_index = date_index;
			candidate.start_block = start_block;
			candidate.available_count = 0;
			candidate.maybe_count = 0;

			for (participant_index = 0; participant_index < participant_count;
				 ++participant_index) {
				int status = participant_status(&participants[participant_index],
												date_index, start_block,
												blocks_needed);
				if (status == AVAILABLE) {
					++candidate.available_count;
				} else if (status == MAYBE) {
					++candidate.maybe_count;
				}
			}

			insert_at = 0;
			while (insert_at < recommendation_count &&
				   !ranks_ahead(&candidate, &recommendations[insert_at])) {
				++insert_at;
			}

			if (insert_at >= recommendation_limit) {
				continue;
			}

			if (recommendation_count < recommendation_limit) {
				++recommendation_count;
			}
			{
				int move_index;
				for (move_index = recommendation_count - 1;
					 move_index > insert_at; --move_index) {
					recommendations[move_index] = recommendations[move_index - 1];
				}
			}
			recommendations[insert_at] = candidate;
		}
	}

	return recommendation_count;
}

static void print_clock_time(int minutes)
{
    minutes %= 24 * 60;
	int hour = minutes / 60;
	int minute = minutes % 60;
	const char *period = hour < 12 ? "AM" : "PM";
	int display_hour = hour % 12;

	if (display_hour == 0) {
		display_hour = 12;
	}
	printf("%d:%02d %s", display_hour, minute, period);
}

void print_recommendations(const char *event_name, const EventDate dates[],
						   int date_count, const Participant participants[],
						   int participant_count, int duration_minutes,
						   const Recommendation recommendations[],
						   int recommendation_count)
{
	int recommendation_index;
	int all_participants_fit = 0;

	(void)date_count;
	printf("\nSuggestions for %s (%d minute meeting)\n", event_name,
		   duration_minutes);

	if (recommendation_count == 0) {
		printf("No candidate time window is long enough for this meeting.\n");
		return;
	}

	for (recommendation_index = 0;
		 recommendation_index < recommendation_count; ++recommendation_index) {
		if (recommendations[recommendation_index].available_count ==
			participant_count) {
			all_participants_fit = 1;
			break;
		}
	}

	if (!all_participants_fit) {
		printf("No option works for everyone. Best options are ranked by the number of available participants.\n");
	}

	for (recommendation_index = 0;
		 recommendation_index < recommendation_count; ++recommendation_index) {
		const Recommendation *recommendation =
			&recommendations[recommendation_index];
		const EventDate *date = &dates[recommendation->date_index];
		int start_minutes = date->start_minutes + recommendation->start_block * 30;
		int end_minutes = start_minutes + duration_minutes;
		int participant_index;
		int first_name;

		printf("\n%d. %s, ", recommendation_index + 1, date->label);
		print_clock_time(start_minutes);
		printf(" - ");
		print_clock_time(end_minutes);
		printf(" | %d available, %d maybe\n",
			   recommendation->available_count, recommendation->maybe_count);

		printf("   Available: ");
		first_name = 1;
		for (participant_index = 0; participant_index < participant_count;
			 ++participant_index) {
			if (participant_status(&participants[participant_index],
								   recommendation->date_index,
								   recommendation->start_block,
								   (duration_minutes + 29) / 30) == AVAILABLE) {
				printf("%s%s", first_name ? "" : ", ",
					   participants[participant_index].name);
				first_name = 0;
			}
		}
		if (first_name) {
			printf("none");
		}
		printf("\n   Maybe: ");
		first_name = 1;
		for (participant_index = 0; participant_index < participant_count;
			 ++participant_index) {
			if (participant_status(&participants[participant_index],
								   recommendation->date_index,
								   recommendation->start_block,
								   (duration_minutes + 29) / 30) == MAYBE) {
				printf("%s%s", first_name ? "" : ", ",
					   participants[participant_index].name);
				first_name = 0;
			}
		}
		if (first_name) {
			printf("none");
		}
		printf("\n");
	}
}
