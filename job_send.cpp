
#include "stratum.h"

static int g_job_next_id = 0;

int job_get_jobid()
{
	CommonLock(&g_job_create_mutex);
	int jobid = ++g_job_next_id;

	CommonUnlock(&g_job_create_mutex);
	return jobid;
}

static void job_mining_notify_buffer(YAAMP_JOB *job, char *buffer)
{
	YAAMP_JOB_TEMPLATE *templ = job->templ;

	if (!strcmp(g_stratum_algo, "lbry")) {
		sprintf(buffer, "{\"id\":null,\"method\":\"mining.notify\",\"params\":["
			"\"%x\",\"%s\",\"%s\",\"%s\",\"%s\",[%s],\"%s\",\"%s\",\"%s\",true]}\n",
			job->id, templ->prevhash_be, templ->claim_be, templ->coinb1, templ->coinb2,
			templ->txmerkles, templ->version, templ->nbits, templ->ntime);
		return;
	} else if (strlen(templ->extradata_hex) == 128) {
		// LUX smart contract state hashes (like lbry extra field, here the 2 root hashes in one)
		sprintf(buffer, "{\"id\":null,\"method\":\"mining.notify\",\"params\":["
			"\"%x\",\"%s\",\"%s\",\"%s\",\"%s\",[%s],\"%s\",\"%s\",\"%s\",true]}\n",
			job->id, templ->prevhash_be, templ->extradata_be, templ->coinb1, templ->coinb2,
			templ->txmerkles, templ->version, templ->nbits, templ->ntime);
		return;
	}
    // equihash job

    /*
        // blockTemplate.js
        this.jobParams = [
                this.jobId,
                util.packUInt32LE(this.rpcData.version).toString('hex'),
                this.prevHashReversed,
                this.merkleRootReversed,
                this.hashReserved, //hashReserved
                util.packUInt32LE(rpcData.curtime).toString('hex'),
                util.reverseBuffer(new Buffer(this.rpcDta.bits, 'hex')).toString('hex'),
                true
            ];
    */

    if (!strcmp(g_stratum_algo, "equihash")) {
        char rev_version[32] = {0};
        char prevHashReversed[65] = {0};
        char merkleRootReversed[65] = {0};
        char rev_ntime[32] = {0};
        char rev_nbits[32] = {0};

        
        string_be(templ->version,rev_version);
        string_be(templ->prevhash_hex,prevHashReversed);
        string_be(templ->mr_hex,merkleRootReversed);
        //std::cerr << "Merkle (sent): " << merkleRootReversed << std::endl;
        
        //memset(merkleRootReversed, 0x30, 64); merkleRootReversed[65] = 0;
        
        string_be(templ->ntime,rev_ntime);
        string_be(templ->nbits,rev_nbits);
        // https://github.com/slushpool/poclbm-zcash/wiki/Stratum-protocol-changes-for-ZCash
        // jobId, version, prevHashReversed, merkleRootReversed, hashReserved (finalsaplingroothash), curtime, nbits
        //sprintf(buffer, "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"%x\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",true ]}\n",
        sprintf(buffer, "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"%x\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\", true]}\n",
		job->id, rev_version, prevHashReversed, merkleRootReversed, templ->extradata_be, rev_ntime, rev_nbits);
        return;
    }

	// standard stratum
    // https://en.bitcoin.it/wiki/Stratum_mining_protocol#mining.notify
	sprintf(buffer, "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"%x\",\"%s\",\"%s\",\"%s\",[%s],\"%s\",\"%s\",\"%s\",true]}\n",
		job->id, templ->prevhash_be, templ->coinb1, templ->coinb2, templ->txmerkles, templ->version, templ->nbits, templ->ntime);
    
}

static YAAMP_JOB *job_get_last(int coinid)
{
	g_list_job.Enter();
    /*
    {   // debug (iterate the list of jobs and print content)
        CLI li = g_list_job.first;
        for (int i = 0; i < g_list_job.count; i++) 
        {
            YAAMP_JOB *job = (YAAMP_JOB *)li->data;
            std::cerr << job->coind->symbol << " " << job->coind->id << std::endl;
            li = li->next;
        }
    }
    */
    
	for(CLI li = g_list_job.first; li; li = li->next) // should be li->next, instead of li->prev, otherwise we will not iterate list
	{
		YAAMP_JOB *job = (YAAMP_JOB *)li->data;
		if(!job_can_mine(job)) continue;
		if(!job->coind) continue;
		if(coinid > 0 && job->coind->id != coinid) continue;

		g_list_job.Leave();
		return job;
	}

	g_list_job.Leave();
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void job_send_last(YAAMP_CLIENT *client)
{
#ifdef NO_EXCHANGE
	// prefer user coin first (if available)
	YAAMP_JOB *job = job_get_last(client->coinid);
	//if(job) debuglog("[1] job_send_last for coind \"%s\" (client->coinid.%d)\n", job->coind->symbol, client->coinid);
	if(!job) job = job_get_last(0);
	//if(job) debuglog("[2] job_send_last for coind \"%s\" (client->coinid.%d)\n", job->coind->symbol, client->coinid);
#else
	YAAMP_JOB *job = job_get_last(0);
#endif
	if(!job) return;
	//if(job) debuglog("[3] job_send_last for coind \"%s\" (client->coinid.%d)\n", job->coind->symbol, client->coinid);
	YAAMP_JOB_TEMPLATE *templ = job->templ;
	client->jobid_sent = job->id;

	char buffer[YAAMP_SMALLBUFSIZE];
	job_mining_notify_buffer(job, buffer);

	socket_send_raw(client->sock, buffer, strlen(buffer));
}

void job_send_jobid(YAAMP_CLIENT *client, int jobid)
{
	YAAMP_JOB *job = (YAAMP_JOB *)object_find(&g_list_job, jobid, true);
	if(!job)
	{
		job_send_last(client);
		return;
	}

	char buffer[YAAMP_SMALLBUFSIZE];
	job_mining_notify_buffer(job, buffer);

	YAAMP_JOB_TEMPLATE *templ = job->templ;
	client->jobid_sent = job->id;

	socket_send_raw(client->sock, buffer, strlen(buffer));
	object_unlock(job);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void job_broadcast(YAAMP_JOB *job)
{
	int s1 = current_timestamp_dms();
	int count = 0;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000; // max time to push to a socket (very fast)

	YAAMP_JOB_TEMPLATE *templ = job->templ;

	char buffer[YAAMP_SMALLBUFSIZE];
	job_mining_notify_buffer(job, buffer);

	g_list_client.Enter();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
		if(client->deleted) continue;
		if(!client->sock) continue;
	//	if(client->reconnecting && client->locked) continue;

		if(client->jobid_next != job->id) continue;
		if(client->jobid_sent == job->id) continue;

		client->jobid_sent = job->id;
		client_add_job_history(client, job->id);

		client_adjust_difficulty(client);

		setsockopt(client->sock->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

		if (socket_send_raw(client->sock, buffer, strlen(buffer)) == -1) {
			int err = errno;
			client->broadcast_timeouts++;
			// too much timeouts, disconnect him
			if (client->broadcast_timeouts >= 3) {
				shutdown(client->sock->sock, SHUT_RDWR);
				clientlog(client, "unable to send job, sock err %d (%d times)", err, client->broadcast_timeouts);
				if(client->workerid && !client->reconnecting) {
				//	CommonLock(&g_db_mutex);
					db_clear_worker(g_db, client);
				//	CommonUnlock(&g_db_mutex);
				}
				object_delete(client);
			}
		}
		count++;
	}

	g_list_client.Leave();
	g_last_broadcasted = time(NULL);

	int s2 = current_timestamp_dms();
	if(!count) return;

	///////////////////////

	uint64_t coin_target = decode_compact(templ->nbits);
	if (templ->nbits && !coin_target) coin_target = 0xFFFF000000000000ULL; // under decode_compact min diff
	double coin_diff = target_to_diff(coin_target);

	debuglog("%s %d - diff %.9f job %x to %d/%d/%d clients, hash %.3f/%.3f in %.1f ms\n", job->name,
		templ->height, coin_diff, job->id, count, job->count, g_list_client.count, job->speed, job->maxspeed, 0.1*(s2-s1));

//	for(int i=0; i<templ->auxs_size; i++)
//	{
//		if(!templ->auxs[i]) continue;
//		YAAMP_COIND *coind_aux = templ->auxs[i]->coind;
//
//		unsigned char target_aux[1024];
//		binlify(target_aux, coind_aux->aux.target);
//
//		uint64_t coin_target = get_hash_difficulty(target_aux);
//		double coin_diff = target_to_diff(coin_target);
//
//		debuglog("%s %d - diff %.9f chainid %d [%d]\n", coind_aux->symbol, coind_aux->height, coin_diff,
//				coind_aux->aux.chainid, coind_aux->aux.index);
//	}

}







//	double maxhash = 0;
//	if(job->remote)
//	{
//		sprintf(name, "JOB%d%s (%.3f)", job->remote->id, job->remote->nonce2size == 2? "*": "", job->remote->speed_avg);
//		maxhash = job->remote->speed;
//	}
//	else
//	{
//		strcpy(name, job->coind->symbol);
//		for(int i=0; i<templ->auxs_size; i++)
//		{
//			if(!templ->auxs[i]) continue;
//			YAAMP_COIND *coind_aux = templ->auxs[i]->coind;
//
//			sprintf(name_auxs+strlen(name_auxs), ", %s %d", coind_aux->symbol, templ->auxs[i]->height);
//		}
//
//		maxhash = coind_nethash(job->coind)*coind_profitability(job->coind)/(g_current_algo->profit? g_current_algo->profit: 1);
//	}

