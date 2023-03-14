/* Toy-Shell/src/command.h */
#ifndef COMMAND_SENTRY
#define COMMAND_SENTRY

struct command {
    char *cmd_name;
    int argc;
    char **argv;
    int argv_cap;
    int stdin_fd, stdout_fd;
};

enum command_res_type { exited, killed, failed, noproc, not_implemented };
struct command_res {
    enum command_res_type type;
    int code;
};

struct command_chain;

typedef void (*command_modifier)(struct command *);

int command_is_empty(struct command *cp);
void free_command(struct command *cp);

struct command_chain *create_cmd_chain();
int cmd_chain_is_empty(struct command_chain *cc);
int cmd_chain_len(struct command_chain *cc);
int cmd_chain_is_background(struct command_chain *cc);
void set_cmd_chain_to_background(struct command_chain *cc);
struct command *add_cmd_to_chain(struct command_chain *cc);
int add_arg_to_last_chain_cmd(struct command_chain *cc, char *arg);
struct command *get_first_cmd_in_chain(struct command_chain *cc);
struct command *get_last_cmd_in_chain(struct command_chain *cc);
int delete_first_cmd_from_chain(struct command_chain *cc);
void free_command_chain(struct command_chain *cc);

void map_to_all_cmds_in_chain(struct command_chain *cc, command_modifier func);
int chain_contains_cmd(struct command_chain *cc, const char *cmd_name);

/* debug */
void print_cmd_chain(struct command_chain *cc);

#endif
