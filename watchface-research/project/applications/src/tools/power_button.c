#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <linux/types.h>
#include <stdio.h>
#include <barcelona/Barc_Gpio.h>

#define ID_BUTTON 1
#define ID_LOWBATT 8192

int pollStatus(int devHWStatus)
{
	HARDWARE_STATUS hwstatus;

	if (ioctl(devHWStatus, IOR_HWSTATUS, &hwstatus) < 0) {
		fprintf(stderr, "Could not read hardware status: %s", strerror(errno));
		return -1;
	}

	return hwstatus.u8InputStatus;
}

int resetStatus(int devHWStatus)
{
	UINT32 status = 0;

	if (ioctl(devHWStatus, IOW_RESET_ONOFF_STATE, &status) < 0) {
		fprintf(stderr, "Could not read hardware status: %s", strerror(errno));
		return -1;
	}

	return 0;
}

int waitButton(int devHWStatus, char *envp[], int poweroff_button,
	       char *bjob, char *ljob)
{
	int result, cpid;
	char *job;
	UINT32 status = 0;

	while (1) {
		resetStatus(devHWStatus);
		sleep(1);
		while (((result = pollStatus(devHWStatus)) & (ID_LOWBATT | ID_BUTTON)) == 0) {
			sleep(1);
		}
		job = ljob;
		if (result & ID_BUTTON) {
			if (poweroff_button) {
				sync();
				if (ioctl(devHWStatus, IOW_POKE_RESET_BUTTON, &status) < 0) {
					fprintf(stderr, "Could not power off: %s\n", strerror(errno));
					return -1;
				}
				fprintf(stderr, "Power-off request returned unexpectedly\n");
				return -1;
			}
			job = bjob;
		}

		if (job == NULL) {
			if ((result & ID_BUTTON) == 0) {
				continue;
			}
			return result;
		}
		cpid = fork();
		if (cpid == -1) {
			perror("fork");
			return -1;
		}
		if (cpid == 0) {
			execve(job, NULL, envp);
			perror(job);
			return -1;
		}
		wait(NULL);
	}
	return 0;
}

int main(int argc, char **argv, char *envp[])
{
	int fd = open("/dev/hwstatus", O_RDONLY);
	if (fd == -1) {
		perror("/dev/hwstatus");
		return 2;
	}

	if (argc > 1) {
		if (strncmp(argv[1], "-r", 2) == 0) {
			if (resetStatus(fd)) {
				return -1;
			}
			printf("Resetting ON/OFF state: OK\n");
		} else if (strncmp(argv[1], "-b", 2) == 0) {
			return waitButton(fd, envp, 0,
					  (argc > 2 ? argv[2] : NULL),
					  (argc > 3 ? argv[3] : NULL));
		} else if (strncmp(argv[1], "-p", 2) == 0) {
			return waitButton(fd, envp, 1, NULL,
					  (argc > 2 ? argv[2] : NULL));
		} else {
			printf("Usage: %s [-r]\n", argv[0]);
			printf("Usage: %s -b [button_command [low_batt_command]]\n", argv[0]);
			printf("Usage: %s -p [low_batt_command]\n", argv[0]);
			return -1;
		}
	} else {
		printf("Input status: %d\n", pollStatus(fd));
	}

	return 0;
}
