#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW 4
#define MIN_HISTORY_FOR_FULL_SCORE 8
#define MIN_HISTORY_FOR_MONITOR 4
#define HIGH_RISK_THRESHOLD -40.0
#define MEDIUM_RISK_THRESHOLD -20.0
#define MIN_BASELINE_AVG 10.0
#define VOLATILITY_THRESHOLD 0.35

#define MAX_ACCOUNTS 32
#define MAX_WEEKS 52
#define MAX_ID_LEN 32
#define MAX_NAME_LEN 128
#define MAX_TIER_LEN 32
#define MAX_REASON_LEN 256

typedef struct {
    char account_id[MAX_ID_LEN];
    char account_name[MAX_NAME_LEN];
    char plan_tier[MAX_TIER_LEN];
    int weekly_logins[MAX_WEEKS];
    int weeks;
} Account;

typedef struct {
    char risk_label[32];
    char reason[MAX_REASON_LEN];
    double recent_avg;
    double previous_avg;
    double trend_pct;
    double volatility_ratio;
    int below_baseline_weeks;
    int history_weeks;
    int should_flag;
} Analysis;

static void skip_whitespace(const char **cursor) {
    while (**cursor != '\0' && isspace((unsigned char) **cursor)) {
        *cursor += 1;
    }
}

static int expect_char(const char **cursor, char expected) {
    skip_whitespace(cursor);
    if (**cursor != expected) {
        return 0;
    }
    *cursor += 1;
    return 1;
}

static int parse_string_value(const char **cursor, char *buffer, size_t buffer_size) {
    size_t length = 0;

    skip_whitespace(cursor);
    if (**cursor != '"') {
        return 0;
    }
    *cursor += 1;

    while (**cursor != '\0' && **cursor != '"') {
        char current = **cursor;

        if (current == '\\') {
            *cursor += 1;
            current = **cursor;
            if (current == '\0') {
                return 0;
            }
        }

        if (length + 1 >= buffer_size) {
            return 0;
        }

        buffer[length++] = current;
        *cursor += 1;
    }

    if (**cursor != '"') {
        return 0;
    }

    buffer[length] = '\0';
    *cursor += 1;
    return 1;
}

static int parse_int_value(const char **cursor, int *value) {
    char *end_ptr;
    long parsed;

    skip_whitespace(cursor);
    parsed = strtol(*cursor, &end_ptr, 10);
    if (end_ptr == *cursor) {
        return 0;
    }

    *value = (int) parsed;
    *cursor = end_ptr;
    return 1;
}

static int parse_weekly_logins(const char **cursor, Account *account) {
    int count = 0;

    if (!expect_char(cursor, '[')) {
        return 0;
    }

    skip_whitespace(cursor);
    if (**cursor == ']') {
        *cursor += 1;
        account->weeks = 0;
        return 1;
    }

    while (**cursor != '\0') {
        if (count >= MAX_WEEKS) {
            return 0;
        }

        if (!parse_int_value(cursor, &account->weekly_logins[count])) {
            return 0;
        }
        count += 1;

        skip_whitespace(cursor);
        if (**cursor == ',') {
            *cursor += 1;
            continue;
        }
        if (**cursor == ']') {
            *cursor += 1;
            account->weeks = count;
            return 1;
        }
        return 0;
    }

    return 0;
}

static int parse_account_object(const char **cursor, Account *account) {
    char key[64];
    int saw_id = 0;
    int saw_name = 0;
    int saw_tier = 0;
    int saw_logins = 0;

    memset(account, 0, sizeof(*account));

    if (!expect_char(cursor, '{')) {
        return 0;
    }

    while (**cursor != '\0') {
        skip_whitespace(cursor);
        if (**cursor == '}') {
            *cursor += 1;
            return saw_id && saw_name && saw_tier && saw_logins;
        }

        if (!parse_string_value(cursor, key, sizeof(key))) {
            return 0;
        }
        if (!expect_char(cursor, ':')) {
            return 0;
        }

        if (strcmp(key, "account_id") == 0) {
            saw_id = parse_string_value(cursor, account->account_id, sizeof(account->account_id));
            if (!saw_id) {
                return 0;
            }
        } else if (strcmp(key, "account_name") == 0) {
            saw_name = parse_string_value(cursor, account->account_name, sizeof(account->account_name));
            if (!saw_name) {
                return 0;
            }
        } else if (strcmp(key, "plan_tier") == 0) {
            saw_tier = parse_string_value(cursor, account->plan_tier, sizeof(account->plan_tier));
            if (!saw_tier) {
                return 0;
            }
        } else if (strcmp(key, "weekly_logins") == 0) {
            saw_logins = parse_weekly_logins(cursor, account);
            if (!saw_logins) {
                return 0;
            }
        } else {
            return 0;
        }

        skip_whitespace(cursor);
        if (**cursor == ',') {
            *cursor += 1;
            continue;
        }
        if (**cursor == '}') {
            *cursor += 1;
            return saw_id && saw_name && saw_tier && saw_logins;
        }
        return 0;
    }

    return 0;
}

static int load_accounts_from_json(const char *path, Account *accounts, int max_accounts) {
    FILE *file;
    long file_size;
    size_t bytes_read;
    char *buffer;
    const char *cursor;
    int count = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open input file: %s\n", path);
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Unable to seek input file: %s\n", path);
        return -1;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        fprintf(stderr, "Unable to determine input file size: %s\n", path);
        return -1;
    }

    rewind(file);
    buffer = (char *) malloc((size_t) file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "Unable to allocate memory for input file.\n");
        return -1;
    }

    bytes_read = fread(buffer, 1, (size_t) file_size, file);
    fclose(file);
    if (bytes_read != (size_t) file_size) {
        free(buffer);
        fprintf(stderr, "Unable to read the full input file: %s\n", path);
        return -1;
    }
    buffer[bytes_read] = '\0';

    cursor = buffer;
    if (!expect_char(&cursor, '[')) {
        free(buffer);
        fprintf(stderr, "Expected top-level JSON array.\n");
        return -1;
    }

    skip_whitespace(&cursor);
    if (*cursor == ']') {
        free(buffer);
        return 0;
    }

    while (*cursor != '\0') {
        if (count >= max_accounts) {
            free(buffer);
            fprintf(stderr, "Too many accounts in input file.\n");
            return -1;
        }

        if (!parse_account_object(&cursor, &accounts[count])) {
            free(buffer);
            fprintf(stderr, "Failed to parse account object at index %d.\n", count);
            return -1;
        }
        count += 1;

        skip_whitespace(&cursor);
        if (*cursor == ',') {
            cursor += 1;
            continue;
        }
        if (*cursor == ']') {
            cursor += 1;
            break;
        }
        free(buffer);
        fprintf(stderr, "Expected ',' or ']' after account object.\n");
        return -1;
    }

    skip_whitespace(&cursor);
    if (*cursor != '\0') {
        free(buffer);
        fprintf(stderr, "Unexpected trailing content after JSON array.\n");
        return -1;
    }

    free(buffer);
    return count;
}

static double average_range(const int *values, int start, int end) {
    double total = 0.0;
    int count = end - start;
    int index;

    for (index = start; index < end; ++index) {
        total += values[index];
    }

    return count > 0 ? total / count : 0.0;
}

static double standard_deviation(const int *values, int count, double mean) {
    double variance = 0.0;
    int index;

    if (count <= 1) {
        return 0.0;
    }

    for (index = 0; index < count; ++index) {
        double delta = values[index] - mean;
        variance += delta * delta;
    }

    variance /= count;
    return sqrt(variance);
}

static int count_below_threshold(const int *values, int start, int end, double threshold) {
    int count = 0;
    int index;

    for (index = start; index < end; ++index) {
        if ((double) values[index] < threshold) {
            count += 1;
        }
    }

    return count;
}

static Analysis analyze_account(const Account *account) {
    Analysis analysis;
    int recent_start;
    int previous_start;
    double full_avg;
    double stddev;

    memset(&analysis, 0, sizeof(analysis));
    analysis.history_weeks = account->weeks;

    if (account->weeks < MIN_HISTORY_FOR_MONITOR) {
        strcpy(analysis.risk_label, "INSUFFICIENT_DATA");
        strcpy(analysis.reason, "Account is too new for a meaningful trend; monitor until at least 4 weeks of history are available.");
        return analysis;
    }

    if (account->weeks < MIN_HISTORY_FOR_FULL_SCORE) {
        full_avg = average_range(account->weekly_logins, 0, account->weeks);
        analysis.recent_avg = average_range(account->weekly_logins, account->weeks - MIN_HISTORY_FOR_MONITOR, account->weeks);
        analysis.previous_avg = full_avg;
        if (full_avg > 0.0) {
            analysis.trend_pct = ((analysis.recent_avg - full_avg) / full_avg) * 100.0;
        }

        strcpy(analysis.risk_label, "MONITOR");
        if (analysis.trend_pct <= HIGH_RISK_THRESHOLD) {
            strcpy(analysis.reason, "Usage is materially below the account's short lifetime average, but the account lacks enough history for a full churn-risk flag.");
        } else {
            strcpy(analysis.reason, "Account has some history, but not enough for a reliable prior-vs-recent comparison.");
        }
        return analysis;
    }

    recent_start = account->weeks - WINDOW;
    previous_start = account->weeks - (WINDOW * 2);
    analysis.recent_avg = average_range(account->weekly_logins, recent_start, account->weeks);
    analysis.previous_avg = average_range(account->weekly_logins, previous_start, recent_start);
    if (analysis.previous_avg > 0.0) {
        analysis.trend_pct = ((analysis.recent_avg - analysis.previous_avg) / analysis.previous_avg) * 100.0;
    }

    full_avg = average_range(account->weekly_logins, 0, account->weeks);
    stddev = standard_deviation(account->weekly_logins, account->weeks, full_avg);
    analysis.volatility_ratio = full_avg > 0.0 ? stddev / full_avg : 0.0;
    analysis.below_baseline_weeks = count_below_threshold(account->weekly_logins, recent_start, account->weeks, analysis.previous_avg);

    if (analysis.previous_avg < MIN_BASELINE_AVG) {
        strcpy(analysis.risk_label, "MONITOR");
        strcpy(analysis.reason, "Recent decline exists, but the historical baseline is too low to treat this as a strong churn signal in v1.");
        return analysis;
    }

    if (analysis.trend_pct <= HIGH_RISK_THRESHOLD && analysis.below_baseline_weeks >= 3) {
        strcpy(analysis.risk_label, "HIGH_RISK");
        strcpy(analysis.reason, "Recent usage dropped sharply versus the prior month and stayed below the historical baseline in most recent weeks.");
        analysis.should_flag = 1;
        return analysis;
    }

    if (analysis.trend_pct <= MEDIUM_RISK_THRESHOLD && analysis.below_baseline_weeks >= 3 && analysis.volatility_ratio <= VOLATILITY_THRESHOLD) {
        strcpy(analysis.risk_label, "MEDIUM_RISK");
        strcpy(analysis.reason, "Usage is trending down in a sustained way, though the decline is less severe than the highest-risk accounts.");
        analysis.should_flag = 1;
        return analysis;
    }

    if (analysis.trend_pct < 0.0) {
        strcpy(analysis.risk_label, "MONITOR");
        strcpy(analysis.reason, "Usage is down somewhat, but not enough to justify an outreach flag under the current thresholds.");
        return analysis;
    }

    strcpy(analysis.risk_label, "HEALTHY");
    strcpy(analysis.reason, "Usage is stable or improving, so there is no churn-risk signal from login activity alone.");
    return analysis;
}

static void print_json_string(const char *value) {
    const char *cursor = value;

    putchar('"');
    while (*cursor != '\0') {
        if (*cursor == '\\' || *cursor == '"') {
            putchar('\\');
        }
        putchar(*cursor);
        cursor += 1;
    }
    putchar('"');
}

static void print_weekly_logins(const Account *account) {
    int index;

    printf("[");
    for (index = 0; index < account->weeks; ++index) {
        if (index > 0) {
            printf(", ");
        }
        printf("%d", account->weekly_logins[index]);
    }
    printf("]");
}

int main(int argc, char **argv) {
    Account accounts[MAX_ACCOUNTS];
    const char *input_path = "data/account_usage.json";
    int account_count;
    int index;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [path-to-account-usage.json]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        input_path = argv[1];
    }

    account_count = load_accounts_from_json(input_path, accounts, MAX_ACCOUNTS);
    if (account_count < 0) {
        return 1;
    }

    printf("{\n");
    printf("  \"analysis_version\": \"v1-rule-based\",\n");
    printf("  \"source_file\": ");
    print_json_string(input_path);
    printf(",\n");
    printf("  \"window_weeks\": %d,\n", WINDOW);
    printf("  \"accounts\": [\n");

    for (index = 0; index < account_count; ++index) {
        Analysis analysis = analyze_account(&accounts[index]);
        printf("    {\n");
        printf("      \"account_id\": ");
        print_json_string(accounts[index].account_id);
        printf(",\n");
        printf("      \"account_name\": ");
        print_json_string(accounts[index].account_name);
        printf(",\n");
        printf("      \"plan_tier\": ");
        print_json_string(accounts[index].plan_tier);
        printf(",\n");
        printf("      \"history_weeks\": %d,\n", analysis.history_weeks);
        printf("      \"recent_avg\": %.2f,\n", analysis.recent_avg);
        printf("      \"previous_avg\": %.2f,\n", analysis.previous_avg);
        printf("      \"trend_pct\": %.2f,\n", analysis.trend_pct);
        printf("      \"volatility_ratio\": %.2f,\n", analysis.volatility_ratio);
        printf("      \"below_baseline_weeks\": %d,\n", analysis.below_baseline_weeks);
        printf("      \"should_flag\": %s,\n", analysis.should_flag ? "true" : "false");
        printf("      \"risk_label\": ");
        print_json_string(analysis.risk_label);
        printf(",\n");
        printf("      \"reason\": ");
        print_json_string(analysis.reason);
        printf(",\n");
        printf("      \"weekly_logins\": ");
        print_weekly_logins(&accounts[index]);
        printf("\n    }");
        if (index < account_count - 1) {
            printf(",");
        }
        printf("\n");
    }

    printf("  ]\n");
    printf("}\n");

    return 0;
}
