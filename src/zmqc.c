/* 0MQ PUB-SUB connection
   (C)Copyright 2013 Simon Urbanek
   Licensed under GPL-2|3

   When opened as "r[b]" it is 0MQ-SUB,
   when opened as "{a|w}[b]" it is 0MQ-PUB
*/

#include <zmq.h>
#include <string.h>
#include <stdlib.h>

#include <Rinternals.h>
#include <R_ext/Connections.h>
#if ! defined(R_CONNECTIONS_VERSION) || R_CONNECTIONS_VERSION != 1
#error "Unsupported connections API version"
#endif

static void *context;

typedef struct zqmc_private {
    void *sub;
    zmq_msg_t msg;
    int msg_ptr;
} zmqc_t;

static void zmqc_close(Rconnection c) {
    zmqc_t *cc = (zmqc_t*) c->private;
    if (cc) {
	if (cc->sub) {
	    if (cc->msg_ptr > -1) {
		cc->msg_ptr = -1;
		zmq_msg_close(&(cc->msg));
	    }
	    zmq_close(cc->sub);
	    cc->sub = 0;
	}
	c->isopen = 0;
    }
}

static Rboolean zmqc_open(Rconnection c) {
    zmqc_t *cc = (zmqc_t*) c->private;
    if (cc) {
	if (c->isopen)
	    Rf_error("connection is already open");
	if (c->mode[0] == 'r') {
	    if (c->mode[1] == '+')
		Rf_error("0MQ SUB can only have 'r' or 'rb' mode");
	    c->text = (c->mode[1] == 'b') ? FALSE : TRUE;
	    if (!(cc->sub = zmq_socket(context, ZMQ_SUB)))
		Rf_error("cannot open 0MQ SUB socket");
	    if (zmq_connect(cc->sub, c->description))
		Rf_error("cannot connect to %s", c->description);
	    if (zmq_setsockopt(cc->sub, ZMQ_SUBSCRIBE, "", 0))
		Rf_error("unable to subscribe");
	    c->canwrite = FALSE;
	    c->canread = TRUE;
	} else if (c->mode[0] == 'a' || c->mode[0] == 'w') {
	    c->text = (c->mode[1] == 'b') ? FALSE : TRUE;
	    if (!(cc->sub = zmq_socket(context, ZMQ_PUB)))
		Rf_error("cannot open 0MQ PUB socket");
	    if (zmq_bind(cc->sub, c->description))
		Rf_error("cannot bind to %s", c->description);
	    c->canwrite = TRUE;
	    c->canread = FALSE;
	} else Rf_error("invalid mode for 0MQ PUB/SUB connection");
	c->isopen = TRUE;
	return TRUE;
    }
    return FALSE;
}

static size_t zmqc_read(void *buf, size_t sz, size_t ni, Rconnection c) {
    zmqc_t *cc = (zmqc_t*) c->private;
    size_t len, req = sz * ni;
    if (!cc) Rf_error("invalid 0MQ connection");
    if (cc->msg_ptr == -1) { /* get new message */
	int rc;
	if (!cc->sub) Rf_error("no valid subscription");
	rc = zmq_msg_init(&(cc->msg));
	if (rc == 0) {
	    rc = zmq_recv(cc->sub, &(cc->msg), 0);
	    if (rc == EINTR)
		return 0; /* non-fatal */
	    else if (rc)
		Rf_error("0MQ-SUB read error");
	    cc->msg_ptr = 0;
	} else Rf_error("cannot initialize 0MQ message");
    }
    len = zmq_msg_size(&(cc->msg)) - cc->msg_ptr;
    if (len < req) { /* reduce the requirement to a multiple of sz that we can satisfy */
	ni = len / sz;
	if (ni == 0) { /* FIXME: what to do? We may be able to satisfy if we pull another msg ... */
	    return 0;
	}
	req = ni * sz;
    }
    memcpy(buf, ((unsigned char*)zmq_msg_data(&(cc->msg))) + cc->msg_ptr, req);
    if (len > req)
	cc->msg_ptr += req;
    else {
	cc->msg_ptr = -1;
	zmq_msg_close(&(cc->msg));
    }
    return ni;
}

/* needed to support text mode */
static int zmqc_fgetc(Rconnection c) {
    zmqc_t *cc = (zmqc_t*) c->private;
    int res;
    if (!cc) Rf_error("invalid 0MQ connection");
    if (cc->msg_ptr == -1) {
	unsigned char x[2];
	if (zmqc_read(x, 1, 1, c) != 1) return -1;
	return x[0];
    }
    res = ((unsigned char*)zmq_msg_data(&(cc->msg)))[cc->msg_ptr++];
    if (cc->msg_ptr >= zmq_msg_size(&(cc->msg))) {
	cc->msg_ptr = -1;
        zmq_msg_close(&(cc->msg));
    }
    return res;
}

static size_t zmqc_write(const void *buf, size_t sz, size_t ni, Rconnection c) {
    zmqc_t *cc = (zmqc_t*) c->private;
    size_t req = sz * ni;
    zmq_msg_t msg;
    if (!cc || !cc->sub) Rf_error("invalid 0MQ connection");
    zmq_msg_init_size(&msg, req);
    memcpy(zmq_msg_data(&msg), buf, req);
    /* FIXME: check result? */
    zmq_send(cc->sub, &msg, 0);
    zmq_msg_close(&msg);
    return ni;
}

SEXP zmqc_pubsub(SEXP sURL, SEXP sMode) {
    Rconnection con;
    zmqc_t *cc;
    SEXP rc;
    if (TYPEOF(sURL) != STRSXP || LENGTH(sURL) != 1)
	Rf_error("invalid 0MQ URL specification");
    if (TYPEOF(sMode) != STRSXP || LENGTH(sMode) != 1)
	Rf_error("invalid mode specification");    
    if (!context) {
	context = zmq_init(1);
	if (!context)
	    Rf_error("cannot initialize 0MQ context");
    }
    rc = R_new_custom_connection(CHAR(STRING_ELT(sURL, 0)),
				 CHAR(STRING_ELT(sMode, 0)),
				 "zmqc", &con);
    cc = malloc(sizeof(zmqc_t));
    if (!cc) Rf_error("cannot allocate 0QM private context");
    cc->sub = 0;
    cc->msg_ptr = -1;
    con->private = cc;
    con->canseek = FALSE;
    con->blocking = TRUE;
    con->open = zmqc_open;
    con->close = zmqc_close;
    con->read = zmqc_read;
    con->fgetc = zmqc_fgetc;
    con->write = zmqc_write;
    if (con->mode[0] && !con->open(con)) /* auto-open */
	Rf_error("cannot open the connection");
    return rc;
}
