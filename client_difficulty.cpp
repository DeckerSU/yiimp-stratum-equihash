
#include "stratum.h"

double client_normalize_difficulty(double difficulty)
{
	if(difficulty < g_stratum_min_diff) difficulty = g_stratum_min_diff;
	else if(difficulty < 1) difficulty = floor(difficulty*1000/2)/1000*2;
	else if(difficulty > 1) difficulty = floor(difficulty/2)*2;
	if(difficulty > g_stratum_max_diff) difficulty = g_stratum_max_diff;
	return difficulty;
}

void client_record_difficulty(YAAMP_CLIENT *client)
{
	if(client->difficulty_remote)
	{
		client->last_submit_time = current_timestamp();
		return;
	}

	int e = current_timestamp() - client->last_submit_time;
	if(e < 500) e = 500;
	int p = 5;

	client->shares_per_minute = (client->shares_per_minute * (100 - p) + 60*1000*p/e) / 100;
	client->last_submit_time = current_timestamp();

//	debuglog("client->shares_per_minute %f\n", client->shares_per_minute);
}

void client_change_difficulty(YAAMP_CLIENT *client, double difficulty)
{
	if(difficulty <= 0) return;

	difficulty = client_normalize_difficulty(difficulty);
	if(difficulty <= 0) return;

//	debuglog("change diff to %f %f\n", difficulty, client->difficulty_actual);
	if(difficulty == client->difficulty_actual) return;

	uint64_t user_target = diff_to_target(difficulty);
	if(user_target >= YAAMP_MINDIFF && user_target <= YAAMP_MAXDIFF)
	{
		client->difficulty_actual = difficulty;
		client_send_difficulty(client, difficulty);
	}
}

void client_adjust_difficulty(YAAMP_CLIENT *client)
{
	if(client->difficulty_remote) {
		client_change_difficulty(client, client->difficulty_remote);
		return;
	}

	if(client->shares_per_minute > 100)
		client_change_difficulty(client, client->difficulty_actual*4);

	else if(client->difficulty_fixed)
		return;

	else if(client->shares_per_minute > 25)
		client_change_difficulty(client, client->difficulty_actual*2);

	else if(client->shares_per_minute > 20)
		client_change_difficulty(client, client->difficulty_actual*1.5);

	else if(client->shares_per_minute <  5)
		client_change_difficulty(client, client->difficulty_actual/2);
}

int client_send_difficulty(YAAMP_CLIENT *client, double difficulty)
{
//	debuglog("%s diff %f\n", client->sock->ip, difficulty);
	client->shares_per_minute = YAAMP_SHAREPERSEC;

    if (g_current_algo->name && !strcmp(g_current_algo->name,"equihash")) 
        {
                // send mindiff on client_authorize (temp)
                
                // ZEC uses a different scale to compute diff... 
                // sample targets to diff (stored in the reverse byte order in work->target)
                // 0007fff800000000000000000000000000000000000000000000000000000000 is stratum diff 32
                // 003fffc000000000000000000000000000000000000000000000000000000000 is stratum diff 4
                // 00ffff0000000000000000000000000000000000000000000000000000000000 is stratum diff 1

                // 00000f0f0f0f0f0f0f0000000000000000000000000000000000000000000000 is stratum diff 4351.93 (136)
                // 00000f0fffffffffffffffffffffffffffffffffffffffffffffffffffffffff is stratum diff 4352 (136)
                
                uint8_t equi_target[32] = { 0 };
                char target_str[65]; target_str[64] = 0;
                char target_str_be[65]; target_str_be[64] = 0;

                diff_to_target_equi((uint32_t *)equi_target, difficulty);  
               	hexlify(target_str, equi_target, 32);
                string_be(target_str, target_str_be);
                // debuglog("target_str    : %s\n", target_str);
                // debuglog("target_str_be : %s\n", target_str_be );
               
                client_call(client, "mining.set_target", "[\"%s\"]", target_str_be);
        }
    else
        {
            if(difficulty >= 1)
                client_call(client, "mining.set_difficulty", "[%.0f]", difficulty);
            else
                client_call(client, "mining.set_difficulty", "[%.3f]", difficulty);
            return 0;
        }
}

void client_initialize_difficulty(YAAMP_CLIENT *client)
{
	char *p = strstr(client->password, "d=");
	char *p2 = strstr(client->password, "decred=");
	if(!p || p2) return;

	double diff = client_normalize_difficulty(atof(p+2));
	uint64_t user_target = diff_to_target(diff);

//	debuglog("%016llx target\n", user_target);
	if(user_target >= YAAMP_MINDIFF && user_target <= YAAMP_MAXDIFF)
	{
		client->difficulty_actual = diff;
		client->difficulty_fixed = true;
	}

}






