
//  http://stackoverflow.com/questions/11337368/acoustic-echo-cancellation-aec-in-embedded-software/18628834#18628834


typedef struct aec {
	unsigned long long lastaudibleframe; /* time stamp of last audible frame */
	unsigned short aiws; /* Average mike input when speaker is playing */
	unsigned short aiwos; /*Average mike input when speaker ISNT playing */
	unsigned long long t_aiws, t_aiwos; /* Internal running total (sum of PCM) */
	unsigned int c_aiws, c_aiwos; /* Internal counters for number of frames for averaging */
	unsigned long lockthreadid; /* Thread ID with lock */
	int stlc; /* Same thread lock-count */
} AEC;

#define ECHO_THRESHOLD 300 /* Time to keep suppression alive after last audible frame */
#define ONE_MINUTE 3000 /* 3000 20ms samples */
#define AVG_PERIOD 250 /* 250 20ms samples */
#define ABS(x) (x>0?x:-x)

char removeecho(AEC *ec, short *aecinput) {
	int tas = 0; /* Average absolute amplitude in this signal */
	int i = 0;
	unsigned long long *tot = 0;
	unsigned int *ctr = 0;
	unsigned short *avg = 0;
	char suppressframe = 0;
	lockecho(ec);
	if (ec->lastaudibleframe + ECHO_THRESHOLD > GetTickCount64()) { /* If we're still within the threshold for echo (speaker state is ON) */
		tot = &ec->t_aiws;
		ctr = &ec->c_aiws;
		avg = &ec->aiws;
	} else { /* If we're outside the threshold for echo (speaker state is OFF) */
		tot = &ec->t_aiwos;
		ctr = &ec->c_aiwos;
		avg = &ec->aiwos;
	}
	for (; i < 160; i++) {
		tas += ABS(aecinput[i]);
	}
	tas /= 160;
	if (tas > 200) {
		(*tot) += tas;
		(*avg) = (unsigned short) ((*tot) / ((*ctr) ? (*ctr) : 1));
		(*ctr)++;
		if ((*ctr) > AVG_PERIOD) {
			(*tot) = (*avg);
			(*ctr) = 0;
		}
	}
	if ((avg == &ec->aiws)) {
		tas -= ec->aiwos;
		if (tas < 0) {
			tas = 0;
		}
		if (((unsigned short) tas > (ec->aiws * 1.5))
				&& ((unsigned short) tas >= ec->aiwos) && (ec->aiwos != 0)) {
			suppressframe = 0;
		} else {
			suppressframe = 1;
		}
	}
	if (suppressframe) { /* Silence frame */
		memset(aecinput, 0, 320);
	}
	unlockecho(ec);
	return suppressframe;
}

AEC *initecho(void) {
	AEC *ec = 0;
	ec = (AEC *) malloc(sizeof(AEC));
	memset(ec, 0, sizeof(AEC));
	ec->aiws = 200; /* Just a default guess as to what the average amplitude would be */
	return ec;
}



void audioin(AEC *ec, short *frame) {
	unsigned int tas = 0; /* Total sum of all audio in frame (absolute value) */
	int i = 0;
	for (; i < 160; i++)
		tas += ABS(frame[i]);
	tas /= 160; /* 320 byte frames muLaw */
	if (tas > 300) { /* I assume this is audiable */
		lockecho(ec);
		ec->lastaudibleframe = GetTickCount64();
		unlockecho(ec);
	}
	return;
}





