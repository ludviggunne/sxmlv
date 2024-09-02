#include <stdio.h>
#include <expat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

enum {
	C_BLACK,
	C_RED,
	C_GREEN,
	C_YELLOW,
	C_BLUE,
	C_MAGENTA,
	C_CYAN,
	C_WHITE,
	C_COUNT,
};

#define C_DEFAULT 9

struct style {
	int fg, bg;
	struct style *next;
};

FILE *outf = NULL;
struct style *sty_head;

void usage(FILE *f, const char *name)
{
	fprintf(f, "usage: %s <file>\n", name);
}

void push_style(int fg, int bg)
{
	struct style *s;
	s = malloc(sizeof(*s));
	s->fg = 30 + fg;
	s->bg = 40 + bg;
	s->next = sty_head;
	sty_head = s;
}

void pop_style(void)
{
	if (sty_head)
		sty_head = sty_head->next;
}

void apply_style(void)
{
	if (!sty_head)
		return;
	fprintf(outf, "\x1b[%d;%dm", sty_head->fg, sty_head->bg);
}

char *load_file(char *path, size_t *size)
{
	FILE *f;
	char *buf;

	f = fopen(path, "r");
	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = malloc(*size);
	if (!buf) {
		fclose(f);
		return NULL;
	}

	char *ptr = buf;
	while (ptr < buf + *size) {
		size_t s = fread(ptr, 1, buf + *size - ptr, f);
		if (s == 0) {
			free(buf);
			fclose(f);
			return NULL;
		}
		ptr += s;
	}

	return buf;
}

void cleanup(void)
{
}

void print_parse_error_and_exit(XML_Parser p)
{
	enum XML_Error c = XML_GetErrorCode(p);
	fprintf(stderr, "parse error: %s\n", XML_ErrorString(c));;
	cleanup();
	exit(1);
}

size_t hash(const char *str)
{
	size_t hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

static XMLCALL void start_handler(void *userdata, const XML_Char *name,
				  const XML_Char **attrs)
{
	(void)userdata;
	(void)attrs;
	push_style(hash(name) % C_COUNT, C_DEFAULT);
	apply_style();
}

static XMLCALL void end_handler(void *userdata, const XML_Char *name)
{
	(void)userdata;
	(void)name;
	pop_style();
	apply_style();
}

static XMLCALL void char_handler(void *userdata, const XML_Char *s, int len)
{
	(void)userdata;
	(void)fwrite(s, 1, len, outf);
}

int main(int argc, char **argv)
{
	char *src;
	size_t size;
	XML_Parser p;
	enum XML_Status xml_status;

	if (argc < 2) {
		usage(stderr, argv[0]);
		exit(1);
	}
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		perror("pipe");
		exit(1);
	}

	pid_t pid = fork();

	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) {
		if (dup2(pipefd[0], STDIN_FILENO) < 0) {
			perror("dup2");
			exit(1);
		}

		char *cmd[] = { strdup("less"), strdup("-r"), NULL };
		if (execvp(cmd[0], cmd) < 0) {
			perror("execvp");
			exit(1);
		}
	}

	outf = fdopen(pipefd[1], "w");
	if (!outf) {
		perror("fdopen");
		exit(1);
	}

	src = load_file(argv[1], &size);
	if (!src) {
		perror("load_file");
		exit(1);
	}

	push_style(C_DEFAULT, C_DEFAULT);

	p = XML_ParserCreate(NULL);
	if (!p) {
		fprintf(stderr, "unable to create parser\n");
	}

	XML_SetStartElementHandler(p, start_handler);
	XML_SetEndElementHandler(p, end_handler);
	XML_SetCharacterDataHandler(p, char_handler);

	xml_status = XML_Parse(p, src, size, XML_TRUE);

	if (xml_status != XML_STATUS_OK) {
		print_parse_error_and_exit(p);
	}

	push_style(C_DEFAULT, C_DEFAULT);
	apply_style();

	fclose(outf);

	int status;
	wait(&status);
	(void)status;
}
