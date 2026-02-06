#include <unistd.h>

int main(void)
{
	const char* msg = "[rlwsd] HELLO!\n";
	write(1, msg, __builtin_strlen(msg));
	for (;;)
	{
		pause();
	}
}
