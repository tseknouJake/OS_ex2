// Jake Lockitch, 807630
/*
 * ex2.c
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <string.h>
#include <signal.h>

#define HTTP_OK 200L
#define REQUEST_TIMEOUT_SECONDS 2L

#define URL_OK 0
#define URL_UNKNOWN -1
#define URL_ERROR -2

#define MAX_PROCESSES 1024

const char URL_PREFIX[] = "http";

typedef struct
{
	double sum;
	int amount, unknown;
} ResultStruct;

void usage()
{
	fprintf(stderr, "usage:\n\t./ex2 num_of_processes FILENAME\n");
	exit(EXIT_FAILURE);
}

double check_url(const char *url)
{
	CURL *curl;
	CURLcode res;
	double response_time = URL_UNKNOWN;

	curl = curl_easy_init();

	if (strncmp(url, URL_PREFIX, strlen(URL_PREFIX)) != 0)
	{
		return URL_ERROR;
	}

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_TIMEOUT_SECONDS);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); /* do a HEAD request */

		res = curl_easy_perform(curl);
		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &response_time);
		}

		curl_easy_cleanup(curl);
	}

	return response_time;
}

void serial_checker(const char *filename)
{

	ResultStruct results = {0};

	FILE *toplist_file;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	double res;

	toplist_file = fopen(filename, "r");

	if (toplist_file == NULL)
	{
		exit(EXIT_FAILURE);
	}

	while ((read = getline(&line, &len, toplist_file)) != -1)
	{
		if (read == -1)
		{
			perror("unable to read line from file");
		}
		line[read - 1] = '\0'; /* null-terminate the URL */
		if (URL_UNKNOWN == (res = check_url(line)))
		{
			results.unknown++;
		}
		else if (res == URL_ERROR)
		{
			printf("Illegal url detected, exiting now\n");
			exit(0);
		}
		else
		{
			results.sum += res;
			results.amount++;
		}
	}

	free(line);
	fclose(toplist_file);

	if (results.amount > 0)
	{
		printf("%.4f Average response time from %d sites, %d Unknown\n",
			   results.sum / results.amount,
			   results.amount,
			   results.unknown);
	}
	else
	{
		printf("No Average response time from 0 sites, %d Unknown\n", results.unknown);
	}
}

/**
 * @define - handle single worker that run on child process
 */
void worker_checker(int worker_id, int num_of_workers, const char *filename, int pipe_write_fd)
{
	/*
	 * TODO: this checker function should operate almost like serial_checker(), except:
	 * 1. Only processing a distinct subset of the lines (hint: think Modulo)
	 * 2. Writing the results back to the parent using the pipe_write_fd (i.e. and not to the screen)
	 * 3. If an URL_ERROR returned, all processes (parent and children) should exit immediatly and an error message should be printed (as in 'serial_checker')
	 */

	ResultStruct results = {0};

	double res;
	FILE *toplist_file;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int line_number = 0;

	// TODO

	// opening the file
	toplist_file = fopen(filename, "r");

	// error handling if the file is null
	if (toplist_file == NULL)
	{
		exit(EXIT_FAILURE);
	}

	// go over all the lines
	while ((read = getline(&line, &len, toplist_file)) != -1)
	{

		// TODO
		// check if the line should be processed by this worker
		if (line_number % num_of_workers == worker_id)
		{
			if (read == -1)
			{
				perror("unable to read line from file");
			}
			line[read - 1] = '\0'; /* null-terminate the URL */
			// how were handing the unknown urls
			if (URL_UNKNOWN == (res = check_url(line)))
			{
				results.unknown++;
			}
			else if (res == URL_ERROR)
			{
				printf("Illegal url detected, exiting now\n");
				exit(0);
			}
			else
			{
				results.sum += res;
				results.amount++;
			}
		}

		// increase line number always
		line_number++;
	}

	// TODO

	// free the line and close the file
	free(line);
	fclose(toplist_file);

	// write the results back to the parent
	write(pipe_write_fd, &results, sizeof(ResultStruct));
	close(pipe_write_fd); // close the pipe
}

/**
 * Handle separate the work between process and merge the results
 */
void parallel_checker(int num_of_processes, const char *filename)
{
	int worker_id;
	int pipefd[2];

	ResultStruct results = {0};
	ResultStruct results_buffer = {0};

	// initialize  pipe
	pipe(pipefd);

	// Start num_of_processes new workers
	for (worker_id = 0; worker_id < num_of_processes; ++worker_id)
	{

		// TODO - fork the children and call worker_checker.
		// Possible implementation: Let worker_checker on which rows to perform work (from file).

		// fork the child
		int pid = fork();

		if (pid < 0)
		{
			perror("fork failed");
			exit(EXIT_FAILURE);
		}
		else if (pid == 0)
		{
			// child process
			close(pipefd[0]); // close the read end
			worker_checker(worker_id, num_of_processes, filename, pipefd[1]);
			exit(EXIT_SUCCESS);
		}
		else // if pid>0
		{
			// father process
			close(pipefd[1]); // close the write end
		}

		// wait(0)?
	}

	// TODO
	wait(0); // wait for all the children to finish

	for (worker_id = 0; worker_id < num_of_processes; ++worker_id)
	{
		// TODO - sum the results

		read(pipefd[0], &results_buffer, sizeof(results_buffer));
		results.sum += results_buffer.sum;
		results.amount += results_buffer.amount;
		results.unknown += results_buffer.unknown;
	}

	// print the total results
	if (results.amount > 0)
	{
		printf("%.4f Average response time from %d sites, %d Unknown\n",
			   results.sum / results.amount,
			   results.amount,
			   results.unknown);
	}
	else
	{
		printf("No Average response time from 0 sites, %d Unknown\n", results.unknown);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		usage();
	}
	else if (atoi(argv[1]) == 1)
	{
		serial_checker(argv[2]);
	}
	else
		parallel_checker(atoi(argv[1]), argv[2]);

	return EXIT_SUCCESS;
}
