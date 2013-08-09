int
main (int argc, char **argv)
{
#if 0
    struct sockaddr_in local_address,
		       remote_address,
		       client_address,
		       restriction_address;
    int 	local_socket,
		client_socket;
    socklen_t	addrlen;
    int 	optval;
    pthread_t	thread;
    ProxyThreadArgs *thread_args;
    int 	restricted;

    if (argc < 4) {
	print_usage ();
	return -1;
    }

    if (argc > 4) {
	if (argc != 5) {
	    print_usage ();
	    return -1;
	}

	if (set_address (argv [4], "0", &restriction_address)) {
	    fprintf (stderr, "Bad host name\n");
	    return -1;
	}

	restricted = 1;
    } else
	restricted = 0;

    if (set_address (argv [1], argv [2], &remote_address)) {
	fprintf (stderr, "Bad destination\n");
	return -1;
    }

    local_socket = socket (AF_INET, SOCK_STREAM, 0);
    if (local_socket == -1) {
	perror ("socket");
	return -1;
    }

    optval = 1;
    if (setsockopt (local_socket,
		    SOL_SOCKET, SO_REUSEADDR,
		    &optval, sizeof optval))
	perror ("setsockopt");

    if (set_address (NULL, argv [3], &local_address)) {
	fprintf (stderr, "Bad local service/port\n");
	return -1;
    }

    if (bind (local_socket,
	      (struct sockaddr*) &local_address,
	      sizeof local_address))
    {
	perror ("bind");
	return -1;
    }

    if (listen (local_socket, SOMAXCONN)) {
	perror ("listen");
	return -1;
    }

    while (1) {
	addrlen = sizeof client_address;
	client_socket = accept (local_socket,
				(struct sockaddr*) &client_address,
				&addrlen);
	if (client_socket == -1) {
	    perror ("accept");
	    return -1;
	}

	fprintf (stderr, "ACCEPTED\n");

	if (!restricted ||
	    (restricted &&
	     client_address.sin_addr.s_addr ==
		restriction_address.sin_addr.s_addr))
	{
	    thread_args = malloc (sizeof (ProxyThreadArgs));
	    memcpy (&thread_args->remote_address,
		    &remote_address, sizeof remote_address);
	    thread_args->client_socket = client_socket;

	    if (pthread_create (&thread, NULL,
				(void* (*)(void*)) proxy_thread,
				thread_args))
		perror ("pthread_create");
	    else 
		pthread_detach (thread);
	} else {
	    close (client_socket);
	    fprintf (stderr, "RESTRICTED\n");
	}
    }

#endif
    return 0;
}

