/*
 * Soft:        Keepalived is a failover program for the LVS project
 *              <www.linuxvirtualserver.org>. It monitor & manipulate
 *              a loadbalanced server pool using multi-layer checks.
 *
 * Part:        Main program structure.
 *
 * Version:     $Id: main.c,v 1.1.2 2003/09/08 01:18:41 acassen Exp $
 *
 * Author:      Alexandre Cassen, <acassen@linux-vs.org>
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2001, 2002, 2003 Alexandre Cassen, <acassen@linux-vs.org>
 */

#include "main.h"
#include "watchdog.h"

/* Daemon stop sequence */
static void
stop_keepalived(void)
{
	syslog(LOG_INFO, "Stopping " VERSION_STRING);
	/* Just cleanup memory & exit */
	thread_destroy_master(master);

	pidfile_rm(KEEPALIVED_PID_FILE);

#ifdef _DEBUG_
	keepalived_free_final("Parent process");
#endif

	/*
	 * Reached when terminate signal catched.
	 * finally return from system
	 */
	closelog();
	exit(0);
}

/* Daemon init sequence */
static void
start_keepalived(void)
{
#ifdef _WITH_LVS_
	/* start healthchecker child */
	start_check_child();
#endif
#ifdef _WITH_VRRP_
	/* start vrrp child */
	start_vrrp_child();
#endif
}

/* SIGHUP handler */
void
sighup(int sig)
{
	/* Set the reloading flag */
	SET_RELOAD;

	/* Signal child process */
	if (vrrp_child > 0)
		kill(vrrp_child, SIGHUP);
	if (checkers_child > 0)
		kill(checkers_child, SIGHUP);
}

/* Terminate handler */
void
sigend(int sig)
{
	int status;
	sigset_t mask;

	/* register the terminate thread */
	syslog(LOG_INFO, "Terminating on signal");
	thread_add_terminate_event(master);

	/*
	 * Signal child process.
	 * Disable and unblock the SIGCHLD handler
	 * so that wait() works.
	 */
	signal_ignore(SIGCHLD);
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	if (vrrp_child > 0) {
		kill(vrrp_child, SIGTERM);
		waitpid(vrrp_child, &status, WNOHANG);
	}
	if (checkers_child > 0) {
		kill(checkers_child, SIGTERM);
		waitpid(checkers_child, &status, WNOHANG);
	}
}

/* Initialize signal handler */
void
signal_init(void)
{
	signal_set(SIGHUP, sighup);
	signal_set(SIGINT, sigend);
	signal_set(SIGTERM, sigend);
	signal_set(SIGKILL, sigend);
	signal_noignore_sigchld();
}

/* Usage function */
static void
usage(const char *prog)
{
	fprintf(stderr, VERSION_STRING);
	fprintf(stderr,
		"\nUsage:\n"
		"  %s\n"
		"  %s -n\n"
		"  %s -f keepalived.conf\n"
		"  %s -d\n"
		"  %s -h\n" "  %s -v\n\n", prog, prog, prog, prog, prog, prog);
	fprintf(stderr,
		"Commands:\n"
		"Either long or short options are allowed.\n"
		"  %s --dont-release-vrrp  -V    Dont remove VRRP VIPs & VROUTEs on daemon stop.\n"
		"  %s --dont-release-ipvs  -I    Dont remove IPVS topology on daemon stop.\n"
		"  %s --dont-fork          -n    Dont fork the daemon process.\n"
		"  %s --use-file           -f    Use the specified configuration file.\n"
		"                                Default is /etc/keepalived/keepalived.conf.\n"
		"  %s --wdog-vrrp          -R    Define VRRP watchdog polling delay. (default=5s)\n"
		"  %s --wdog-check         -H    Define checkers watchdog polling delay. (default=5s)\n"
		"  %s --dump-conf          -d    Dump the configuration data.\n"
		"  %s --log-console        -l    Log message to local console.\n"
		"  %s --log-detail         -D    Detailed log messages.\n"
		"  %s --help               -h    Display this short inlined help screen.\n"
		"  %s --version            -v    Display the version number\n",
		prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

/* Command line parser */
static void
parse_cmdline(int argc, char **argv)
{
	poptContext context;
	char *optarg = NULL;
	int c;

	struct poptOption options_table[] = {
		{"version", 'v', POPT_ARG_NONE, NULL, 'v'},
		{"help", 'h', POPT_ARG_NONE, NULL, 'h'},
		{"log-console", 'l', POPT_ARG_NONE, NULL, 'l'},
		{"log-detail", 'D', POPT_ARG_NONE, NULL, 'D'},
		{"dont-release-vrrp", 'V', POPT_ARG_NONE, NULL, 'V'},
		{"dont-release-ipvs", 'I', POPT_ARG_NONE, NULL, 'I'},
		{"dont-fork", 'n', POPT_ARG_NONE, NULL, 'n'},
		{"dump-conf", 'd', POPT_ARG_NONE, NULL, 'd'},
		{"use-file", 'f', POPT_ARG_STRING, &optarg, 'f'},
		{"wdog-vrrp", 'R', POPT_ARG_STRING, &optarg, 'R'},
		{"wdog-check", 'H', POPT_ARG_STRING, &optarg, 'H'},
		{NULL, 0, 0, NULL, 0}
	};

	context =
	    poptGetContext(PROG, argc, (const char **) argv, options_table, 0);
	if ((c = poptGetNextOpt(context)) < 0) {
		return;
	}

	/* The first option car */
	switch (c) {
	case 'v':
		fprintf(stderr, VERSION_STRING);
		exit(0);
		break;
	case 'h':
		usage(argv[0]);
		exit(0);
		break;
	case 'l':
		debug |= 1;
		break;
	case 'n':
		debug |= 2;
		break;
	case 'd':
		debug |= 4;
		break;
	case 'V':
		debug |= 8;
		break;
	case 'I':
		debug |= 16;
		break;
	case 'D':
		debug |= 32;
		break;
	case 'f':
		conf_file = optarg;
		break;
	case 'R':
		wdog_delay_vrrp = atoi(optarg);
		break;
	case 'H':
		wdog_delay_check = atoi(optarg);
		break;
	}

	/* the others */
	while ((c = poptGetNextOpt(context)) >= 0) {
		switch (c) {
		case 'l':
			debug |= 1;
			break;
		case 'n':
			debug |= 2;
			break;
		case 'd':
			debug |= 4;
			break;
		case 'V':
			debug |= 8;
			break;
		case 'I':
			debug |= 16;
			break;
		case 'D':
			debug |= 32;
			break;
		case 'f':
			conf_file = optarg;
			break;
		case 'R':
			wdog_delay_vrrp = atoi(optarg);
			break;
		case 'H':
			wdog_delay_check = atoi(optarg);
			break;
		}
	}

	/* check unexpected arguments */
	if ((optarg = (char *) poptGetArg(context))) {
		fprintf(stderr, "unexpected argument %s\n", optarg);
		return;
	}

	/* free the allocated context */
	poptFreeContext(context);
}

/* Entry point */
int
main(int argc, char **argv)
{
	/* Init debugging level */
	mem_allocated = 0;
	debug = 0;

	/*
	 * Parse command line and set debug level.
	 * bits 0..7 reserved by main.c
	 */
	parse_cmdline(argc, argv);

	openlog(PROG, LOG_PID | (debug & 1) ? LOG_CONS : 0, LOG_DAEMON);
	syslog(LOG_INFO, "Starting " VERSION_STRING);

	/* Check if keepalived is already running */
	if (keepalived_running()) {
		syslog(LOG_INFO, "daemon is already running");
		syslog(LOG_INFO, "Stopping " VERSION_STRING);
		closelog();
		exit(0);
	}

	/* daemonize process */
	if (!(debug & 2))
		xdaemon(0, 0, 0);

	/* write the pidfile */
	if (!pidfile_write(KEEPALIVED_PID_FILE, getpid())) {
		syslog(LOG_INFO, "Stopping " VERSION_STRING);
		closelog();
		exit(0);
	}

	/* Signal handling initialization  */
	signal_init();

	/* Create the master thread */
	master = thread_make_master();

	/* Init daemon */
	start_keepalived();

	/* Launch the scheduling I/O multiplexer */
	launch_scheduler();

	/* Finish daemon process */
	stop_keepalived();
	exit(0);
}