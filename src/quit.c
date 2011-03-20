#include <stdlib.h>
#include <string.h>
#include "bot.h"

const char command[] = "PRIVMSG";

int create_response(struct irc_message *msg, struct irc_message **messages,
		    int *msg_count)
{
	char *msg_nick, *msg_message;
	msg_nick = strtok(msg->prefix, "!") + 1;
	msg_message = strtok(msg->params, " ");
	msg_message = strtok(NULL, "") + 1;

	*msg_count = 0;
	
	if (strcmp(msg_message, "!QUIT") == 0 && strcmp(msg_nick, "brandonw") == 0) {
		kill_bot();
		messages[0] = create_message(NULL, "QUIT", NULL);
		if (messages[0])
			*msg_count = 1;
	}
	return 0;
}
