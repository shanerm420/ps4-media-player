/* netstat.h - connection state, measured off the frame loop.
 *
 * The console answers two questions cheaply and nothing else is asked: is it
 * connected, and is the link wired or wi-fi. No socket is opened, no address,
 * name or identifier is read or logged. The library calls happen on a worker
 * thread that sleeps between polls, so a slow or absent network can never hold
 * a frame or delay the boot.
 */
#ifndef NETSTAT_H
#define NETSTAT_H

/* Starts the monitor (once). `enabled` comes from config.ini [log]
 * network_status; when it is 0 the state is simply not looked at.
 * `poll_seconds` is the gap between checks. */
void net_start(int enabled, int poll_seconds);

#endif /* NETSTAT_H */
