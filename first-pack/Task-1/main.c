#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <node/openssl/sha.h>

#define LOGIN_LENGTH 6
#define HASH_LENGTH 64

typedef struct
{
    char login[LOGIN_LENGTH + 1];
    char password_hash[HASH_LENGTH + 1];
    // int password;
    int request_limit;
    int request_used;
} User;

User *current_user = NULL;
User *users = NULL;
int user_count = 0;
int users_capacity = 0;

void sha256_hash(char *input, char output[HASH_LENGTH + 1])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input, strlen(input));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[HASH_LENGTH] = '\0';
}

int free_users()
{
    free(users);
    user_count = 0;
    users_capacity = 0;
    return 0;
}

int init_users()
{
    users_capacity = 10;
    users = (User *)malloc(users_capacity * sizeof(User));
    if (!users)
    {
        return -1;
    }
    return 0;
}

int expand_users()
{
    if (user_count >= users_capacity)
    {
        users_capacity *= 2;
        User *new_users = (User *)realloc(users, users_capacity * sizeof(User));
        if (!new_users)
        {
            free_users();
            return -1;
        }
        users = new_users;
    }
    return 0;
}

int save_user_in_database()
{
    FILE *file = fopen("database.txt", "w");
    if (!file)
    {
        printf("ERROR: failed opening database.txt for 'w'\n");
        return -1;
    }

    for (int i = 0; i < user_count; i++)
    {
        if (fprintf(file, "%s %s %d\n", users[i].login, users[i].password_hash, users[i].request_limit) < 0)
        {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

int load_users_from_database()
{
    FILE *file = fopen("database.txt", "r");
    if (!file)
    {
        printf("ERROR: failed opening database.txt for 'r'\n");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char login[LOGIN_LENGTH + 1];
        char password_hash[HASH_LENGTH + 1];
        int request_limit;

        if (sscanf(line, "%6s %s %d", login, password_hash, &request_limit) != 3)
        {
            printf("ERROR: malformed line in database");
        }

        if (expand_users() != 0)
        {
            fclose(file);
            return -1;
        }

        strncpy(users[user_count].login, login, LOGIN_LENGTH);
        strncpy(users[user_count].password_hash, password_hash, HASH_LENGTH);
        users[user_count].request_limit = request_limit;
        users[user_count].request_used = 0;
        user_count++;
    }

    fclose(file);
    return 0;
}

void show_date()
{
    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        printf("ERROR: failed to get current time");
        return;
    }

    struct tm *tm = localtime(&now);
    if (tm == NULL)
    {
        printf("ERROR: failed to convert time\n");
        return;
    }
    printf("Current date: %02d:%02d:%04d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
}

void show_time()
{
    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        perror("ERROR: failed to get current time");
        return;
    }

    struct tm *tm = localtime(&now);
    if (tm == NULL)
    {
        printf("ERROR: failed to convert time\n");
        return;
    }
    printf("Current date: %02d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void show_howmuch(char *time_str, char *flag)
{
    struct tm tm = {0};
    // if (strptime(time_str, "%d:%m:Y", &tm) == NULL)
    // {
    //     printf("Invalid date format\n");
    //     return;
    // }

    int day, month, year;
    if (sscanf(time_str, "%d:%d:%d", &day, &month, &year) == 3)
    {
        tm.tm_mday = day;
        tm.tm_mon = month - 1;
        tm.tm_year = year - 1900;
    }
    // else
    // {
    //     printf("Invalid date format\n");
    // }

    time_t specified_time = mktime(&tm);
    if (specified_time == (time_t)-1)
    {
        printf("ERROR: failed to convert specified time\n");
        return;
    }

    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        printf("ERROR: failed to get current time\n");
        return;
    }

    double diff = difftime(now, specified_time);
    if (diff < 0)
    {
        printf("future\n");
        return;
    }

    if (strcmp(flag, "-s") == 0)
    {
        printf("Seconds passed: %.0f\n", diff);
    }
    else if (strcmp(flag, "-m") == 0)
    {
        printf("Minutes passed: %.0f\n", diff / 60);
    }
    else if (strcmp(flag, "-h") == 0)
    {
        printf("Hours passed: %.0f\n", diff / 3600);
    }
    else if (strcmp(flag, "-y") == 0)
    {
        printf("Years passed: %.2f\n", diff / (3600 * 24 * 365.25));
    }
    else
    {
        printf("Invalid flag\n");
    }
}

int valid_login(char *login)
{
    if (strlen(login) > LOGIN_LENGTH)
    {
        return -1;
    }

    for (int i = 0; login[i]; i++)
    {
        if (!isalnum(login[i]))
        {
            return -1;
        }
    }

    return 0;
}

int valid_password(char *password)
{
    for (int i = 0; password[i] != '\0'; i++)
    {
        if (!isdigit(password[i]))
        {
            return -1;
        }
    }

    int num = atoi(password);

    if (num < 0 || num > 100000)
    {
        return -1;
    }

    return 0;
}

int set_sanctions(char *username, int number_limit)
{
    int confirmation;
    printf("Enter confrimation code >>> ");

    if (scanf("%d", &confirmation) != 1 || confirmation != 12345)
    {
        printf("Invalid confirmation code\n");
        while (getchar() != '\n')
        {
            return -1;
        }
    }

    for (int i = 0; i < user_count; i++)
    {
        if (strcmp(users[i].login, username) == 0)
        {
            users[i].request_limit = number_limit;
            if (save_user_in_database() != 0)
            {
                printf("ERROR: failed to save sanctions\n");
                return -1;
            }
            printf("Sanctions set for %s: max %d requests\n", username, number_limit);
            return 0;
        }
    }
    printf("User not found");
    return -1;
}

int register_user()
{
    char login[LOGIN_LENGTH + 1];
    char *password;

    printf("Enter login >>> ");
    if (scanf("%6s", login) != 1)
    {
        printf("Invalid login input\n");
        while (getchar() != '\n')
        {
            return -1;
        }
    }

    if (valid_login(login) == -1)
    {
        printf("Invalid login input, max length login is 6\n");
        return -1;
    }

    for (int i = 0; i < user_count; i++)
    {
        if (strcmp(users[i].login, login) == 0)
        {
            printf("User already exists\n");
            return -1;
        }
    }

    password = getpass("Enter password >>> ");

    if (valid_password(password) == -1)
    {
        printf("Invalid password input, password must be 0...100000\n");
        return -1;
    }

    if (expand_users() != 0)
    {
        return -1;
    }

    strcpy(users[user_count].login, login);
    sha256_hash(password, users[user_count].password_hash);
    users[user_count].request_limit = -1;
    users[user_count].request_used = 0;
    user_count++;

    if (save_user_in_database() != 0)
    {
        printf("ERROR: failed to save user in databse\n");
        user_count--;
        return -1;
    }

    printf("Registration successful!\n\n");
    return 0;
}

int login_user()
{
    char login[LOGIN_LENGTH + 1];
    char *password;
    char input_hash[HASH_LENGTH + 1];

    printf("Login >>> ");
    if (scanf("%6s", login) != 1)
    {
        printf("Invalid login\n");
        while (getchar() != '\n')
        {
            return -1;
        }
    }

    password = getpass("Password >>> ");

    for (int i = 0; i < user_count; i++)
    {
        if (strcmp(users[i].login, login) == 0)
        {
            sha256_hash(password, input_hash);
            if (strcmp(users[i].password_hash, input_hash) == 0)
            {
                current_user = &users[i];
                printf("Login successful!\nWelcome, %s!\n\n", login);
                return 0;
            }
        }
    }

    printf("Invalid login or password\n\n");
    return -1;
}

int show_menu_auth()
{
    int choice;
    while (1)
    {
        printf("Please select number:\n");
        printf("1. Login\n2. Register\n3. Exit\n>>> ");

        if (scanf("%d", &choice) != 1)
        {
            printf("Unknown number\n");
            while (getchar() != '\n')
            {
                continue;
            }
        }

        switch (choice)
        {
        case 1:
            if (login_user() == 0)
            {
                return 0;
            }
            break;
        case 2:
            register_user();
            break;
        case 3:
            printf("Exit successful!\n");
            exit(0);
        default:
            printf("Invalid choice\n");
            break;
        }
    }
}

void show_menu_command()
{
    char command[64];
    char arg1[64];
    char arg2[64];
    int number_limit;

    printf(">>> ");
    if (scanf("%64s", command) != 1)
    {
        printf("Invalid command\n");
        while (getchar() != '\n')
        {
            return;
        }
    }

    if (current_user->request_limit != -1 && current_user->request_used >= current_user->request_limit)
    {
        printf("Reached request limit\n");
        current_user = NULL;
        return;
    }

    if (strcmp(command, "Time") == 0)
    {
        show_time();
        current_user->request_used++;
    }
    else if (strcmp(command, "Date") == 0)
    {
        show_date();
        current_user->request_used++;
    }
    else if (strcmp(command, "Logout") == 0)
    {
        printf("Bye!\n");
        current_user = NULL;
    }
    else if (strcmp(command, "Howmuch") == 0)
    {
        if (scanf("%63s %63s", arg1, arg2) != 2)
        {
            printf("Invalid arg1 and argv2 for Howmuch\n");
            while (getchar() != '\n')
            {
                return;
            }
        }

        show_howmuch(arg1, arg2);
        current_user->request_used++;
    }
    else if (strcmp(command, "Sanctions") == 0)
    {
        if (scanf("%63s %d", arg1, &number_limit) != 2)
        {
            printf("Invalid arg1 and number for Sanctions\n");
            while (getchar() != '\n')
            {
                return;
            }
        }
        if (set_sanctions(arg1, number_limit))
        {
            current_user->request_used++;
        }
    }
    else
    {
        printf("Invalid command\n");
    }
}

int main()
{
    if (init_users() != 0)
    {
        printf("ERROR: failed malloc for users\n");
        return -1;
    }

    if (load_users_from_database() != 0)
    {
        printf("ERROR: failed realloc for users\n");
    }

    while (1)
    {
        if (!current_user)
        {
            show_menu_auth();
        }
        else
        {
            show_menu_command();
        }
    }

    free_users();
    return 0;
}
