#ifndef HELLO_H
#define HELLO_H

#define HELLO_VERSION	"0.0.1"

#define HELLO_FAMILY_NAME "hello"

enum hello_commands {
	HELLO_CMD_UNSPEC = 0,
	HELLO_CMD_ASK,
	HELLO_CMD_RES,
};

enum hello_attrs{
	HELLO_ATTR_UNSPEC=0,
	HELLO_ATTR_MSG,
};

#endif /*HELLO_H*/
