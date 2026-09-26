#define _GNU_SOURCE

/*
 * Rollback-capable updater for the OpenTom watchface and its configuration.
 * This intentionally does not update the kernel, ttsystem, or startup files.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ROOT_PATH "/mnt/sdcard/opentom"
#define PACKAGE_PATH "/mnt/sdcard/opentom/update"
#define FILE_COUNT 2
#define SHA256_BLOCK_SIZE 64

typedef unsigned int sha_word;

typedef struct {
	sha_word state[8];
	unsigned long long bit_count;
	unsigned char block[SHA256_BLOCK_SIZE];
	unsigned int block_length;
} Sha256;

static const char *relative_paths[FILE_COUNT] = {
	"bin/watchface",
	"etc/watchface.cfg"
};

static const sha_word sha_constants[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
	0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
	0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
	0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
	0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
	0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
	0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static sha_word
rotate_right(sha_word value, unsigned int count)
{
	return (value >> count) | (value << (32U - count));
}

static sha_word
load_word(const unsigned char *bytes)
{
	return ((sha_word)bytes[0] << 24) |
	       ((sha_word)bytes[1] << 16) |
	       ((sha_word)bytes[2] << 8) |
	       (sha_word)bytes[3];
}

static void
store_word(unsigned char *bytes, sha_word value)
{
	bytes[0] = (unsigned char)(value >> 24);
	bytes[1] = (unsigned char)(value >> 16);
	bytes[2] = (unsigned char)(value >> 8);
	bytes[3] = (unsigned char)value;
}

static void
sha256_transform(Sha256 *context, const unsigned char *block)
{
	sha_word words[64];
	sha_word a, b, c, d, e, f, g, h;
	unsigned int i;

	for (i = 0; i < 16; i++)
		words[i] = load_word(block + i * 4);
	for (i = 16; i < 64; i++) {
		sha_word s0 = rotate_right(words[i - 15], 7) ^
			      rotate_right(words[i - 15], 18) ^
			      (words[i - 15] >> 3);
		sha_word s1 = rotate_right(words[i - 2], 17) ^
			      rotate_right(words[i - 2], 19) ^
			      (words[i - 2] >> 10);
		words[i] = words[i - 16] + s0 + words[i - 7] + s1;
	}

	a = context->state[0];
	b = context->state[1];
	c = context->state[2];
	d = context->state[3];
	e = context->state[4];
	f = context->state[5];
	g = context->state[6];
	h = context->state[7];

	for (i = 0; i < 64; i++) {
		sha_word s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
			      rotate_right(e, 25);
		sha_word choose = (e & f) ^ ((~e) & g);
		sha_word temp1 = h + s1 + choose + sha_constants[i] + words[i];
		sha_word s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
			      rotate_right(a, 22);
		sha_word majority = (a & b) ^ (a & c) ^ (b & c);
		sha_word temp2 = s0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
	context->state[5] += f;
	context->state[6] += g;
	context->state[7] += h;
}

static void
sha256_init(Sha256 *context)
{
	static const sha_word initial_state[8] = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
	};

	memcpy(context->state, initial_state, sizeof(initial_state));
	context->bit_count = 0;
	context->block_length = 0;
}

static void
sha256_update(Sha256 *context, const unsigned char *data, size_t length)
{
	size_t offset = 0;

	while (offset < length) {
		size_t available = SHA256_BLOCK_SIZE - context->block_length;
		size_t amount = length - offset;
		if (amount > available)
			amount = available;
		memcpy(context->block + context->block_length, data + offset, amount);
		context->block_length += (unsigned int)amount;
		offset += amount;
		if (context->block_length == SHA256_BLOCK_SIZE) {
			sha256_transform(context, context->block);
			context->bit_count += SHA256_BLOCK_SIZE * 8U;
			context->block_length = 0;
		}
	}
}

static void
sha256_final(Sha256 *context, unsigned char digest[32])
{
	unsigned long long total_bits =
		context->bit_count + (unsigned long long)context->block_length * 8U;
	unsigned int i;

	context->block[context->block_length++] = 0x80;
	if (context->block_length > 56) {
		while (context->block_length < SHA256_BLOCK_SIZE)
			context->block[context->block_length++] = 0;
		sha256_transform(context, context->block);
		context->block_length = 0;
	}
	while (context->block_length < 56)
		context->block[context->block_length++] = 0;
	for (i = 0; i < 8; i++)
		context->block[63 - i] = (unsigned char)(total_bits >> (i * 8));
	sha256_transform(context, context->block);

	for (i = 0; i < 8; i++)
		store_word(digest + i * 4, context->state[i]);
}

static int
sha256_file(const char *path, char output[65])
{
	FILE *stream;
	unsigned char buffer[4096];
	unsigned char digest[32];
	Sha256 context;
	size_t amount;
	unsigned int i;
	static const char hex[] = "0123456789abcdef";

	stream = fopen(path, "rb");
	if (!stream) {
		perror(path);
		return -1;
	}
	sha256_init(&context);
	while ((amount = fread(buffer, 1, sizeof(buffer), stream)) > 0)
		sha256_update(&context, buffer, amount);
	if (ferror(stream)) {
		fprintf(stderr, "watchface_update: read failed for %s\n", path);
		fclose(stream);
		return -1;
	}
	if (fclose(stream) != 0) {
		perror(path);
		return -1;
	}
	sha256_final(&context, digest);
	for (i = 0; i < 32; i++) {
		output[i * 2] = hex[digest[i] >> 4];
		output[i * 2 + 1] = hex[digest[i] & 15];
	}
	output[64] = '\0';
	return 0;
}

static void
make_path(char *output, size_t output_size, const char *base,
	  const char *relative)
{
	if (snprintf(output, output_size, "%s/%s", base, relative) >=
	    (int)output_size) {
		fprintf(stderr, "watchface_update: path is too long\n");
		exit(1);
	}
}

#ifndef UPDATE_TEST
static int
check_mount(void)
{
	FILE *stream;
	char line[512];
	char device[128], mountpoint[128], filesystem[32], options[128];

	stream = fopen("/proc/mounts", "r");
	if (!stream) {
		perror("/proc/mounts");
		return -1;
	}
	while (fgets(line, sizeof(line), stream)) {
		if (sscanf(line, "%127s %127s %31s %127s",
			   device, mountpoint, filesystem, options) != 4)
			continue;
		if (strcmp(mountpoint, "/mnt/sdcard") != 0)
			continue;
		if (strcmp(filesystem, "vfat") != 0 &&
		    strcmp(filesystem, "msdos") != 0 &&
		    strcmp(filesystem, "fat") != 0) {
			fprintf(stderr, "watchface_update: /mnt/sdcard is not FAT\n");
			fclose(stream);
			return -1;
		}
		if (strncmp(options, "rw", 2) != 0 &&
		    strstr(options, ",rw") == NULL) {
			fprintf(stderr, "watchface_update: /mnt/sdcard is read-only\n");
			fclose(stream);
			return -1;
		}
		fclose(stream);
		return 0;
	}
	fclose(stream);
	fprintf(stderr, "watchface_update: /mnt/sdcard is not a mounted volume\n");
	return -1;
}
#endif

static int
is_regular_file(const char *path)
{
	struct stat status;

	if (lstat(path, &status) < 0)
		return 0;
	return S_ISREG(status.st_mode);
}

static int
verify_manifest(const char *package_path, char payload_paths[FILE_COUNT][512])
{
	char manifest_path[512];
	FILE *manifest;
	char line[256];
	char seen[FILE_COUNT] = { 0, 0 };
	unsigned int lines = 0;

	make_path(manifest_path, sizeof(manifest_path), package_path, "SHA256SUMS");
	manifest = fopen(manifest_path, "r");
	if (!manifest) {
		perror(manifest_path);
		return -1;
	}

	while (fgets(line, sizeof(line), manifest)) {
		char *name;
		char expected[65];
		char actual[65];
		unsigned int i, file_index;
		size_t line_length = strlen(line);

		if (line_length == 0 || line[line_length - 1] != '\n') {
			fprintf(stderr, "watchface_update: malformed manifest line\n");
			fclose(manifest);
			return -1;
		}
		line[line_length - 1] = '\0';
		if (line[0] == '\0')
			continue;
		if (strlen(line) < 67 || line[64] != ' ' || line[65] != ' ') {
			fprintf(stderr, "watchface_update: malformed manifest entry\n");
			fclose(manifest);
			return -1;
		}
		for (i = 0; i < 64; i++) {
			if (!isxdigit((unsigned char)line[i])) {
				fprintf(stderr, "watchface_update: invalid SHA-256\n");
				fclose(manifest);
				return -1;
			}
			expected[i] = (char)tolower((unsigned char)line[i]);
		}
		expected[64] = '\0';
		name = line + 66;
		file_index = FILE_COUNT;
		for (i = 0; i < FILE_COUNT; i++)
			if (strcmp(name, relative_paths[i]) == 0)
				file_index = i;
		if (file_index == FILE_COUNT || seen[file_index]) {
			fprintf(stderr, "watchface_update: unexpected manifest path %s\n",
				name);
			fclose(manifest);
			return -1;
		}
		seen[file_index] = 1;
		lines++;

		make_path(payload_paths[file_index], 512, package_path, "payload");
		{
			char full_path[512];
			make_path(full_path, sizeof(full_path),
				  payload_paths[file_index], relative_paths[file_index]);
			strcpy(payload_paths[file_index], full_path);
		}
		if (!is_regular_file(payload_paths[file_index])) {
			fprintf(stderr, "watchface_update: missing or unsafe payload %s\n",
				name);
			fclose(manifest);
			return -1;
		}
		if (sha256_file(payload_paths[file_index], actual) < 0) {
			fclose(manifest);
			return -1;
		}
		if (strcmp(expected, actual) != 0) {
			fprintf(stderr, "watchface_update: checksum mismatch for %s\n",
				name);
			fclose(manifest);
			return -1;
		}
	}
	if (ferror(manifest)) {
		perror(manifest_path);
		fclose(manifest);
		return -1;
	}
	fclose(manifest);
	if (lines != FILE_COUNT || !seen[0] || !seen[1]) {
		fprintf(stderr, "watchface_update: manifest must list exactly two files\n");
		return -1;
	}
	return 0;
}

static int
copy_file(const char *source, const char *destination, mode_t mode)
{
	FILE *input;
	FILE *output;
	char buffer[4096];
	size_t amount;
	int result = 0;

	input = fopen(source, "rb");
	if (!input) {
		perror(source);
		return -1;
	}
	output = fopen(destination, "wb");
	if (!output) {
		perror(destination);
		fclose(input);
		return -1;
	}
	while ((amount = fread(buffer, 1, sizeof(buffer), input)) > 0) {
		if (fwrite(buffer, 1, amount, output) != amount) {
			perror(destination);
			result = -1;
			break;
		}
	}
	if (ferror(input)) {
		fprintf(stderr, "watchface_update: read failed for %s\n", source);
		result = -1;
	}
	if (fflush(output) != 0 || fsync(fileno(output)) != 0) {
		perror(destination);
		result = -1;
	}
	if (fclose(output) != 0) {
		perror(destination);
		result = -1;
	}
	if (fclose(input) != 0) {
		perror(source);
		result = -1;
	}
	if (result == 0 && chmod(destination, mode) < 0) {
		perror(destination);
		result = -1;
	}
	if (result < 0)
		unlink(destination);
	return result;
}

#ifndef UPDATE_TEST
static int
watchface_running(void)
{
	DIR *directory;
	struct dirent *entry;
	int found = 0;

	directory = opendir("/proc");
	if (!directory) {
		perror("/proc");
		return -1;
	}
	while ((entry = readdir(directory)) != NULL) {
		char *end;
		long pid = strtol(entry->d_name, &end, 10);
		char path[128];
		char command[256];
		FILE *stream;
		size_t length;
		char *base;

		if (*entry->d_name == '\0' || *end != '\0' || pid <= 1 ||
		    pid == (long)getpid())
			continue;
		snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
		stream = fopen(path, "r");
		if (!stream)
			continue;
		length = fread(command, 1, sizeof(command) - 1, stream);
		fclose(stream);
		if (length == 0)
			continue;
		command[length] = '\0';
		base = strrchr(command, '/');
		base = base ? base + 1 : command;
		if (strcmp(base, "watchface") == 0)
			found = 1;
	}
	closedir(directory);
	return found;
}

static int
stop_watchface(void)
{
	DIR *directory;
	struct dirent *entry;
	int attempt;

	directory = opendir("/proc");
	if (!directory) {
		perror("/proc");
		return -1;
	}
	while ((entry = readdir(directory)) != NULL) {
		char *end;
		long pid = strtol(entry->d_name, &end, 10);
		char path[128], command[256], *base;
		FILE *stream;
		size_t length;

		if (*entry->d_name == '\0' || *end != '\0' || pid <= 1 ||
		    pid == (long)getpid())
			continue;
		snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
		stream = fopen(path, "r");
		if (!stream)
			continue;
		length = fread(command, 1, sizeof(command) - 1, stream);
		fclose(stream);
		if (length == 0)
			continue;
		command[length] = '\0';
		base = strrchr(command, '/');
		base = base ? base + 1 : command;
		if (strcmp(base, "watchface") == 0 &&
		    kill((pid_t)pid, SIGTERM) < 0 && errno != ESRCH) {
			perror("watchface_update: stopping watchface");
			closedir(directory);
			return -1;
		}
	}
	closedir(directory);

	for (attempt = 0; attempt < 5; attempt++) {
		int running = watchface_running();
		if (running < 0)
			return -1;
		if (!running)
			return 0;
		sleep(1);
	}
	directory = opendir("/proc");
	if (!directory) {
		perror("/proc");
		return -1;
	}
	while ((entry = readdir(directory)) != NULL) {
		char *end;
		long pid = strtol(entry->d_name, &end, 10);
		char path[128], command[256], *base;
		FILE *stream;
		size_t length;

		if (*entry->d_name == '\0' || *end != '\0' || pid <= 1 ||
		    pid == (long)getpid())
			continue;
		snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
		stream = fopen(path, "r");
		if (!stream)
			continue;
		length = fread(command, 1, sizeof(command) - 1, stream);
		fclose(stream);
		if (length == 0)
			continue;
		command[length] = '\0';
		base = strrchr(command, '/');
		base = base ? base + 1 : command;
		if (strcmp(base, "watchface") == 0 &&
		    kill((pid_t)pid, SIGKILL) < 0 && errno != ESRCH) {
			perror("watchface_update: stopping watchface");
			closedir(directory);
			return -1;
		}
	}
	closedir(directory);

	if (watchface_running() > 0) {
		fprintf(stderr, "watchface_update: previous process did not stop\n");
		return -1;
	}
	return 0;
}

static int
restart_watchface(const char *root)
{
	pid_t child = fork();
	char executable[512];

	if (child < 0) {
		perror("watchface_update: fork");
		return -1;
	}
	make_path(executable, sizeof(executable), root, "bin/watchface");
	if (child == 0) {
		execl(executable, "watchface", (char *)NULL);
		perror(executable);
		_exit(127);
	}
	return 0;
}
#endif

static int
apply_update(const char *root, const char *package_path,
	     char payload_paths[FILE_COUNT][512])
{
	char destination_paths[FILE_COUNT][512];
	char temporary_paths[FILE_COUNT][512];
	char backup_root[512], backup_paths[FILE_COUNT][512];
	char rollback_dir[512], timestamp[32];
	time_t now;
	struct tm *utc;
	unsigned int i, replaced = 0;
	struct stat status;

	for (i = 0; i < FILE_COUNT; i++) {
		make_path(destination_paths[i], sizeof(destination_paths[i]),
			  root, relative_paths[i]);
		if (!is_regular_file(destination_paths[i])) {
			fprintf(stderr, "watchface_update: required destination missing or unsafe: %s\n",
				destination_paths[i]);
			return -1;
		}
		snprintf(temporary_paths[i], sizeof(temporary_paths[i]),
			 "%s.update-%ld", destination_paths[i], (long)getpid());
		if (access(temporary_paths[i], F_OK) == 0) {
			fprintf(stderr, "watchface_update: temporary file already exists: %s\n",
				temporary_paths[i]);
			return -1;
		}
	}

	if (mkdir(root, 0755) < 0 && errno != EEXIST) {
		perror(root);
		return -1;
	}
	make_path(backup_root, sizeof(backup_root), root, "rollback");
	if (mkdir(backup_root, 0755) < 0 && errno != EEXIST) {
		perror(backup_root);
		return -1;
	}
	now = time(NULL);
	utc = gmtime(&now);
	if (!utc || strftime(timestamp, sizeof(timestamp), "watchface-%Y%m%dT%H%M%SZ",
			     utc) == 0) {
		fprintf(stderr, "watchface_update: cannot create backup timestamp\n");
		return -1;
	}
	make_path(rollback_dir, sizeof(rollback_dir), backup_root, timestamp);
	if (mkdir(rollback_dir, 0755) < 0) {
		perror(rollback_dir);
		return -1;
	}
	for (i = 0; i < FILE_COUNT; i++) {
		char backup_file[512];
		snprintf(backup_file, sizeof(backup_file), "%s/%s",
			 rollback_dir, relative_paths[i]);
		{
			char *slash = strrchr(backup_file, '/');
			char parent[512];
			size_t parent_length;
			if (!slash || (size_t)(slash - backup_file) >= sizeof(parent)) {
				fprintf(stderr, "watchface_update: invalid backup path\n");
				return -1;
			}
			parent_length = (size_t)(slash - backup_file);
			memcpy(parent, backup_file, parent_length);
			parent[parent_length] = '\0';
			if (mkdir(parent, 0755) < 0 && errno != EEXIST) {
				perror(parent);
				return -1;
			}
		}
		strcpy(backup_paths[i], backup_file);
		if (stat(destination_paths[i], &status) < 0 ||
		    copy_file(destination_paths[i], backup_paths[i],
			      status.st_mode & 0777) < 0)
			return -1;
	}

	for (i = 0; i < FILE_COUNT; i++) {
		mode_t mode = i == 0 ? 0755 : 0644;
		if (copy_file(payload_paths[i], temporary_paths[i], mode) < 0)
			goto cleanup_temporary;
	}

	for (i = 0; i < FILE_COUNT; i++) {
		if (rename(temporary_paths[i], destination_paths[i]) < 0) {
			perror(destination_paths[i]);
			while (replaced > 0) {
				unsigned int index = --replaced;
				char restore[512];
				snprintf(restore, sizeof(restore), "%s.restore-%ld",
					 destination_paths[index], (long)getpid());
				if (copy_file(backup_paths[index], restore,
					      index == 0 ? 0755 : 0644) < 0 ||
				    rename(restore, destination_paths[index]) < 0) {
					fprintf(stderr,
						"watchface_update: rollback failed for %s; backup remains at %s\n",
						destination_paths[index], backup_paths[index]);
				}
			}
			goto cleanup_temporary;
		}
		replaced++;
	}
	(void)package_path;
	printf("Update installed; rollback files are in %s\n", rollback_dir);
	return 0;

cleanup_temporary:
	for (i = 0; i < FILE_COUNT; i++)
		unlink(temporary_paths[i]);
	return -1;
}

int
main(int argc, char **argv)
{
	int apply = 0;
	int i;
	char package_path[512];
	char payload_paths[FILE_COUNT][512];

#ifdef UPDATE_TEST
	const char *root = getenv("WATCHFACE_UPDATE_ROOT");
#else
	const char *root = ROOT_PATH;
#endif

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--apply") == 0)
			apply = 1;
#ifdef UPDATE_TEST
		else {
			fprintf(stderr, "usage: watchface_update [--apply]\n");
			return 2;
		}
#else
		else {
			fprintf(stderr, "usage: watchface_update [--apply]\n");
			return 2;
		}
#endif
	}
#ifdef UPDATE_TEST
	if (!root || root[0] == '\0') {
		fprintf(stderr, "WATCHFACE_UPDATE_ROOT is required in test build\n");
		return 2;
	}
#else
	if (check_mount() < 0)
		return 1;
#endif

	make_path(package_path, sizeof(package_path), root, "update");
	if (verify_manifest(package_path, payload_paths) < 0)
		return 1;
	printf("Update package SHA-256 checks passed.\n");
	if (!apply) {
		printf("Dry run only; use --apply to install.\n");
		return 0;
	}
	if (apply_update(root, package_path, payload_paths) < 0)
		return 1;
#ifdef UPDATE_TEST
	printf("Test build: skipped stopping and restarting any running watchface.\n");
	return 0;
#else
	if (stop_watchface() < 0)
		return 1;
	if (restart_watchface(root) < 0)
		return 1;
	printf("Watchface restarted.\n");
	return 0;
#endif
}
