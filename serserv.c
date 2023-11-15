#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 4444
#define BUFFER_SIZE 1024

ssize_t full_read(int fd, void *buf, size_t count) {
	ssize_t total_bytes_read = 0;
	ssize_t bytes_read;

	while (total_bytes_read < count) {
		bytes_read = read(fd, buf + total_bytes_read, count - total_bytes_read);

		if (bytes_read == -1) {
			perror("Read failed");
			exit(EXIT_FAILURE);
		} else if (bytes_read == 0) {
			exit(0);
		}

		total_bytes_read += bytes_read;
	}

	return total_bytes_read;
}

int main() {
	int socket_fd;
	struct sockaddr_in server_addr;

	// Create socket
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	// Configure server address structure
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	// Connect to server
	if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		perror("Connection failed");
		exit(EXIT_FAILURE);
	}

	printf("Connected to server\n");

	int fd = open("../images/sdc", O_RDONLY);
	if (fd == -1)
	{
		perror("Failed to open image");
		exit(EXIT_FAILURE);
	}

	unsigned char sector[512];
	for (;;)
	{
		unsigned short int lba;
		unsigned short int n;

		full_read(socket_fd, &lba, sizeof(lba));
		full_read(socket_fd, &n, sizeof(n));

		printf("Read %d sector(s) from %d\n", n, lba);
		lseek(fd, lba * 512, SEEK_SET);

		while (n--)
		{
			read(fd, sector, 512);
			write(socket_fd, sector, 512);
		}
	}

	close(socket_fd);

	return 0;
}
