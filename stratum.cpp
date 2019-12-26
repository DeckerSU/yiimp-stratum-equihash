
#include "stratum.h"
#include <signal.h>
#include <sys/resource.h>

CommonList g_list_coind;
CommonList g_list_client;
CommonList g_list_job;
CommonList g_list_remote;
CommonList g_list_renter;
CommonList g_list_share;
CommonList g_list_worker;
CommonList g_list_block;
CommonList g_list_submit;
CommonList g_list_source;

int g_tcp_port;

char g_tcp_server[1024];
char g_tcp_password[1024];

char g_sql_host[1024];
char g_sql_database[1024];
char g_sql_username[1024];
char g_sql_password[1024];
int g_sql_port = 3306;

char g_stratum_coin_include[256];
char g_stratum_coin_exclude[256];

char g_stratum_algo[256];
double g_stratum_difficulty;
double g_stratum_min_diff;
double g_stratum_max_diff;

int g_stratum_max_ttf;
int g_stratum_max_cons = 5000;
bool g_stratum_reconnect;
bool g_stratum_renting;
bool g_stratum_segwit = false;

int g_limit_txs_per_block = 0;

bool g_handle_haproxy_ips = false;
int g_socket_recv_timeout = 600;

bool g_debuglog_client;
bool g_debuglog_hash;
bool g_debuglog_socket;
bool g_debuglog_rpc;
bool g_debuglog_list;
bool g_debuglog_remote;

bool g_autoexchange = true;

uint64_t g_max_shares = 0;
uint64_t g_shares_counter = 0;
uint64_t g_shares_log = 0;

bool g_allow_rolltime = true;
time_t g_last_broadcasted = 0;
YAAMP_DB *g_db = NULL;

pthread_mutex_t g_db_mutex;
pthread_mutex_t g_nonce1_mutex;
pthread_mutex_t g_job_create_mutex;

struct ifaddrs *g_ifaddr;

volatile bool g_exiting = false;

void *stratum_thread(void *p);
void *monitor_thread(void *p);

////////////////////////////////////////////////////////////////////////////////////////

static void scrypt_hash(const char* input, char* output, uint32_t len)
{
	scrypt_1024_1_1_256((unsigned char *)input, (unsigned char *)output);
}

static void scryptn_hash(const char* input, char* output, uint32_t len)
{
	time_t time_table[][2] =
	{
		{2048, 1389306217},
		{4096, 1456415081},
		{8192, 1506746729},
		{16384, 1557078377},
		{32768, 1657741673},
		{65536, 1859068265},
		{131072, 2060394857},
		{262144, 1722307603},
		{524288, 1769642992},
		{0, 0},
	};

	for(int i=0; time_table[i][0]; i++)
		if(time(NULL) < time_table[i+1][1])
		{
			scrypt_N_R_1_256(input, output, time_table[i][0], 1, len);
			return;
		}
}

static void neoscrypt_hash(const char* input, char* output, uint32_t len)
{
	neoscrypt((unsigned char *)input, (unsigned char *)output, 0x80000620);
}

YAAMP_ALGO g_algos[] =
{
	{"equihash", equi_hash, 1, 0, 0}, 

	{"sha256", sha256_double_hash, 1, 0, 0},
	{"scrypt", scrypt_hash, 0x10000, 0, 0},
	{"scryptn", scryptn_hash, 0x10000, 0, 0},
	{"neoscrypt", neoscrypt_hash, 0x10000, 0, 0},

	{"c11", c11_hash, 1, 0, 0},
	{"x11", x11_hash, 1, 0, 0},
	{"x12", x12_hash, 1, 0, 0},
	{"x13", x13_hash, 1, 0, 0},
	{"x14", x14_hash, 1, 0, 0},
	{"x15", x15_hash, 1, 0, 0},
	{"x17", x17_hash, 1, 0, 0},
	{"x22i", x22i_hash, 1, 0, 0},

	{"x11evo", x11evo_hash, 1, 0, 0},
	{"xevan", xevan_hash, 0x100, 0, 0},

	{"x16r", x16r_hash, 0x100, 0, 0},
	{"x16rv2", x16rv2_hash, 0x100, 0, 0},
	{"x16s", x16s_hash, 0x100, 0, 0},
	{"timetravel", timetravel_hash, 0x100, 0, 0},
	{"bitcore", timetravel10_hash, 0x100, 0, 0},
	{"exosis", exosis_hash, 0x100, 0, 0},
	{"hsr", hsr_hash, 1, 0, 0},
	{"hmq1725", hmq17_hash, 0x10000, 0, 0},

	{"jha", jha_hash, 0x10000, 0},

	{"allium", allium_hash, 0x100, 0, 0},
	{"lyra2", lyra2re_hash, 0x80, 0, 0},
	{"lyra2v2", lyra2v2_hash, 0x100, 0, 0},
	{"lyra2v3", lyra2v3_hash, 0x100, 0, 0},
	{"lyra2z", lyra2z_hash, 0x100, 0, 0},
	{"lyra2zz", lyra2zz_hash, 0x100, 0, 0},

	{"bastion", bastion_hash, 1, 0 },
	{"blake", blake_hash, 1, 0 },
	{"blakecoin", blakecoin_hash, 1 /*0x100*/, 0, sha256_hash_hex },
	{"blake2b", blake2b_hash, 1, 0 },
	{"blake2s", blake2s_hash, 1, 0 },
	{"vanilla", blakecoin_hash, 1, 0 },
	{"decred", decred_hash, 1, 0 },

	{"deep", deep_hash, 1, 0, 0},
	{"fresh", fresh_hash, 0x100, 0, 0},
	{"quark", quark_hash, 1, 0, 0},
	{"nist5", nist5_hash, 1, 0, 0},
	{"qubit", qubit_hash, 1, 0, 0},
	{"groestl", groestl_hash, 0x100, 0, sha256_hash_hex }, /* groestlcoin */
	{"dmd-gr", groestl_hash, 0x100, 0, 0}, /* diamond (double groestl) */
	{"myr-gr", groestlmyriad_hash, 1, 0, 0}, /* groestl + sha 64 */
	{"skein", skein_hash, 1, 0, 0},
	{"sonoa", sonoa_hash, 1, 0, 0},
	{"tribus", tribus_hash, 1, 0, 0},
	{"keccak", keccak256_hash, 0x80, 0, sha256_hash_hex },
	{"keccakc", keccak256_hash, 0x100, 0, 0},
	{"hex", hex_hash, 0x100, 0, sha256_hash_hex },
	
	{"phi", phi_hash, 1, 0, 0},
	{"phi2", phi2_hash, 0x100, 0, 0},

	{"polytimos", polytimos_hash, 1, 0, 0},
	{"skunk", skunk_hash, 1, 0, 0},

	{"bmw", bmw_hash, 1, 0, 0},
	{"lbk3", lbk3_hash, 0x100, 0, 0},
	{"lbry", lbry_hash, 0x100, 0, 0},
	{"luffa", luffa_hash, 1, 0, 0},
	{"penta", penta_hash, 1, 0, 0},
	{"rainforest", rainforest_hash, 0x100, 0, 0},
	{"skein2", skein2_hash, 1, 0, 0},
	{"yescrypt", yescrypt_hash, 0x10000, 0, 0},
	{"yescryptR16", yescryptR16_hash, 0x10000, 0, 0 },
	{"yescryptR32", yescryptR32_hash, 0x10000, 0, 0 },
	{"zr5", zr5_hash, 1, 0, 0},

	{"a5a", a5a_hash, 0x10000, 0, 0},
	{"hive", hive_hash, 0x10000, 0, 0},
	{"m7m", m7m_hash, 0x10000, 0, 0},
	{"veltor", veltor_hash, 1, 0, 0},
	{"velvet", velvet_hash, 0x10000, 0, 0},
	{"argon2", argon2a_hash, 0x10000, 0, sha256_hash_hex },
	{"argon2d-dyn", argon2d_dyn_hash, 0x10000, 0, 0 }, // Dynamic Argon2d Implementation
	{"vitalium", vitalium_hash, 1, 0, 0},
	{"aergo", aergo_hash, 1, 0, 0},

	{"sha256t", sha256t_hash, 1, 0, 0}, // sha256 3x

	{"sha256q", sha256q_hash, 1, 0, 0}, // sha256 4x

	{"sib", sib_hash, 1, 0, 0},

	{"whirlcoin", whirlpool_hash, 1, 0, sha256_hash_hex }, /* old sha merkleroot */
	{"whirlpool", whirlpool_hash, 1, 0 }, /* sha256d merkleroot */
	{"whirlpoolx", whirlpoolx_hash, 1, 0, 0},

	{"", NULL, 0, 0},
};

YAAMP_ALGO *g_current_algo = NULL;

YAAMP_ALGO *stratum_find_algo(const char *name)
{
	for(int i=0; g_algos[i].name[0]; i++)
		if(!strcmp(name, g_algos[i].name))
			return &g_algos[i];

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{

    /*
    // DEBUG

    // hdr -> header including nonce (140 bytes)
    // soln -> equihash solution (excluding 3 bytes with size, so 400 bytes length)
    // bool verifyEH(const char *hdr, const char *soln) 

    const char *test_block_header = "040000009645c09c014c192abb653944fe2c8341e3e87f52d43a47131691c78689723800890efe479d0649826b3f72c19fcda72f662e7a2472c847d8cdaf650e658d6709fbc2f4300c01f0b7820d00e3347c8da4ee614674376cbc45359daa54f9b5493efdf5035e23b305202000000600000000000000001f7c000001000000000000000000000000000000fd40050428c5c22c60588351ee372637544d4a74e7fa4177291d25d5280ccaafe34b43de5cfd416fcb703e8171101fdbe40c95324d664ed74eb75c80fb04a3f84ba45f04969604ace065d7d177dd0e48842213cadb16550a8bcd0d0cd595e0be8d40da4cf40514b27b9adbe31510415a0cd2b49bad95428a4e6e81f53d366bdc6f1fb97a95cd5488eabd53c419e329d6e12066d28c9328995c19691f9821a9bbd5c3da6905517f386fc1fb04e5cc100487c20493a387bbd1e5731e9ff517db900fd736261712db697b3c05c2da3f0566e23cb80298061c1a2b7ddf113dd570b139a88f99813cf99ae4761ddc1915ae08c4f863b5d6afe76fc62a4e291abac71687cb617ec8f50f665a73583c4fc9c1d8fb7b60ca3bb146ff5cd9af42da15a3ea31dbb2f172217e71f422a072d0b921e5ddd444f42e40d3c639918cd0309a4b036cb3619a6535c32119e25a583fd2f210fd7aa407ad963b875b825b7afde27147348661e413fcc9811269595a98deb1f9f7a4b25f98dd77bec3f4faa43f3547fc45d5bab2a9fba2d493126ef9da9fe69782483a444249c5dbf5a317df086b32ef53627f5cd8269a0adb796c575f6fe9e73c8212a9132e98bc9b1ec2f70ed7b15578565fc4fc4c563280e7c8c5bb17538ba02316b95f42501edff8a73395d3b8b3f5eb3a782344282bbb5743d94b3b78f45511716412dabd6bdea5b40ec0e17b040523d91fbff6b8e37b0acb430eba327f248d7daa3fdbefbb4e3b03c9ee25f86ef1029d287017743330e3d40459914a8b6959e40e879c1c3d801b1e0a65741ed1480761a8c52f3b4252499c31926dbf19c23f62e9bc39bffde5b40353c75a9e7c87fa494335c94c01da60279b5df557178448499f1a11b96c8e1c71c528764cd3ddfc05e762e0bb1f0e02fab05db61d6cc49049c99f279aec0346bcf3c6894ac671655e04325b617bccc6d590c037138053996a414afd3b301b2c19b9631bd5937ee76206586666968696fd3aa60c16a22af04355a3adee92ee65c52346b67bfe0f2f277fe2be370fe1d3daf9e359da4954c5cca9f85e960a1d3b4f80f8fed9cb756522d23666fefa9bd9fb9216cbbffec915d25eef914674c4c73c96ae777730dc0a7f867b07516c277ea4961d05b3c86e2ef7f9d84322c82ed615dd3591e94674ff685279a6f98bd950db079ce04e118cbea0da762a67a7dd5e12b049dd42be0ab5369d99970820eb3e21145e360e1e2da39c078f0ea890a00e50b04719df14e5223fa6b1784cd21b8f16db6e14ef1f427722dec1d844a72d58a139fe43321e12c1f8f28cc2cdabdff3be2ba712e91eec8f178d25e0b7dff672a3ef9602aa3ca8df0b6b2a799b528924ea3739310b5705a65615349651c9d9bdbd95ae7233add4559eeb83b1c496e3f14ea66fe14391948ce5053963c965029f19d9a9d58a32588cd24f57bcfc8d1b483204578c41132e7bd333ace99d1dd1483b8b5c237722cd94a6c2033af045ddf26fbd0a401717170f364b5ff0fc24d8e98bfefa796e7925a6c8067981c80646fb809a5e11995f2971b815c5a0520a565352f12fcfbf424a9096baaf4e54a413bc32d1a5cd79d8720d54b06c3956c8855370641957721af261bbd793c1119c8c0730058eead7d7175046fb055e633fd669ac0e67bb28180dc26562d621e05d52874139d45291be130b48e983dc5fd78771244e517e22c2391f3f87591b175483f2899c0b68f446b2a3c2b1f1ed2054b1201b476347a80baa9306164ae71c72ed030c70be291716ba5ff9af49d194f35f371be6f505cec80cbc25b81d2a51258058d185f9f884f4f87b832193f01347eb195af3d4414adf97b7a5365725efe9bacd7697512b3f67b364215f8c29954a366e55c499b7098198ac15";
    unsigned char test_block_header_bin[4096] = { 0 };
    binlify(test_block_header_bin, test_block_header);
    // we should pass sol without length bytes
    std::cerr << "verifyEH " << verifyEH((const char *)test_block_header_bin, (const char *)&test_block_header_bin[140 + 3]) << std::endl;
    const char *generated = "04000000a72814d0f1f43a1343bd90b0e56fbb65e47e30e1647f5ed1fcf2926b0011afa05119440df4954c47ac402ffbe8478b8c5edb7e1ac1734d554968f63f2a146b1ffbc2f4300c01f0b7820d00e3347c8da4ee614674376cbc45359daa54f9b5493eb5ae045e54c305208100000100000000000000009106000001000000000000000000000000000000fd40050007330a39d19507da64b34f90a9715e7a2f9876683d330aac78a2f0054503042a2eac7f4a2316bea59811f515bd891f0d272be7e827a6dd1c727de49f1866139544828ae5d9337d31b394bb7e4c41e53e3fa49b00d92137185758d8f97be085895d110639551f25bd57b7d728c016fe08dde6466763e61bbe7660fd26d3055a19dbdea77c0d4af132202fe2cee511351f457d3bcb6c5ca7a6e1df60b9591bc27c83cb13353aaadc02a321418718cc40df2eb4cf9ca940a1f8b4fd72331a673b89888f556ada3723862bab97423a833d0fe9133042b2b69d0742f94bd1d935153827b290dfe9e3161648d68ecd727e816ad3c85de52dc10387cb99c209b0b71596e66bfdb114b17ffcbb564974c43f918b5590c5ee8698fcc378d789ba235be4fb4cea1d9db20d96eb78f791abcd57ced43b83582fbb82a33dd86020b0dc74158a5ae883dbe3823e67c4552b1f7019b7004ee235eb05475c6cbd71795cbb2832f5fefe7b2311fa36956dd4af6f8b2676558dd54a9261d57fdb0d1df25c93e43aaca1f03394ddcaf3cc72ca4bf818b02bfe554c628d0e98e67852e89e7d7a4b37767a10640bb00395bbc5a0507b5404644dab14ad6a63dbdd652b4ceb5233a2d25587d595f779643fc1cb82960df30ee500dd738420e0d9a3126ab1dde0899a637410c11ac89ecba25c3453eb56384ed743f2cb771f5c345403d750bb4f96b3c4d09b035151f01ddd6e6c102e2422e3a5df450dc9ddb530669c20ff1f26194bb7df1a1ce044701957d99f4fceb315593fa7f529f09aaff993616cd7dba8dba59ff50c1027f2f5c32d1b9d67f31dcaefa98e0f58ab1c741549cf5b1ccedd8dfac149331066c3ed271915462306a08f60cd4285e69c68c11f1ceaf0a7788ad9d73094f57ec0de0ed8959c3351253f46f5f7302ffdd7fbd94b364ebbbacc71b99cb80074a286b158f8a55b1df3f1efac117fd6351f5b95096f9f6d7699708adb46622d8c4021b4ab896b851f5ee1bc291d672d4b9eb7d735815902528fc357b90663049bb00521dfbbd167b7a43747a94e45c4946c8b0758f380a18b98dc9d4731586b4d0db54c506b9bbb140c746e38eaefeffcb5368b43e93e56158f394bff2299e779b4e0fc29f84cb25670e512c8a884bbc0ea2c90af7c1dd776575519f45e4de5cfad316d7705be0871a512f59344ddaab9b1b673255020d0428fad8036d624a226562e653a90159bfbe21572b68f76ab98170f04431c5c10c3ae5fd6f4d8634bc9c80250fccb24439c98010ddded0df797efaf6a822a8ed9b6925f0b829fff528acbb51497e5378258c0a1dcb95ab427193e648cd14dd37d9cc0c3ea03675c4157749632ce22a111f892aeba199e7695a26d5232931bd77e7fc04209778ac1e68f2742c6a783efc33ba6e0007f0735013c24d1ce0ca09a8faec187019345e9a78e93cdb602af15c4de50b719386cf497b965ad7635eed2c4211465a5f4ea9c9799efa2359dd5da62af8e391dcd5a1e5248f63a192a00cf76e39f01b665b164dc3ece310c383d349b0352db9916b608aebfd23b85767edba51c82216f1e953e47cc64f29f61690fd91c1751cd5523c7ebd407e68b8dc13fa40d0950190dcc3099cfb445b3df3e6e538d77db201a13e3e5aff70ea8998f9901f174360803db5d844ce758583cc27245fc392a9b188f94a92c111fd3ed35e57e4bf715a7b9273efb780b76ec67d10eeaaca43121e1234315ed1a2a3aaf5f295385e82c2623d7a4a7052892513ff37363bf1547023cc4438a4c5976ada9003781493e799d0ff27ab52925d7de9669f5cf958ac7775fc418ff167efa273c05a820e18986d98ff898615620e2413a38b7faedda1a78aa577f1675eb16a3a3f474d212dd27ed96473d010400008085202f89010000000000000000000000000000000000000000000000000000000000000000ffffffff050289000101ffffffff0110270000000000002321032fa5693dd872b5649a7f02d0381be612987e73b8956b76a0ae2c8eec82e97006ac08f6035e000000000000000000000000000000";
	unsigned char generated_bin[4096] = { 0 };
    binlify(generated_bin, generated);
    std::cerr << "verifyEH " << verifyEH((const char *)generated, (const char *)&generated_bin[140 + 3]) << std::endl;
    */
   
    if(argc < 2)
	{
		printf("usage: %s <algo>\n", argv[0]);
		return 1;
	}

	srand(time(NULL));
	getifaddrs(&g_ifaddr);

	initlog(argv[1]);

#ifdef NO_EXCHANGE
	// todo: init with a db setting or a yiimp shell command
	g_autoexchange = false;
#endif

	char configfile[1024];
	sprintf(configfile, "%s.conf", argv[1]);

	dictionary *ini = iniparser_load(configfile);
	if(!ini)
	{
		debuglog("cant load config file %s\n", configfile);
		return 1;
	}

	g_tcp_port = iniparser_getint(ini, "TCP:port", 3333);
	strcpy(g_tcp_server, iniparser_getstring(ini, "TCP:server", NULL));
	strcpy(g_tcp_password, iniparser_getstring(ini, "TCP:password", NULL));

	strcpy(g_sql_host, iniparser_getstring(ini, "SQL:host", NULL));
	strcpy(g_sql_database, iniparser_getstring(ini, "SQL:database", NULL));
	strcpy(g_sql_username, iniparser_getstring(ini, "SQL:username", NULL));
	strcpy(g_sql_password, iniparser_getstring(ini, "SQL:password", NULL));
	g_sql_port = iniparser_getint(ini, "SQL:port", 3306);

	// optional coin filters (to mine only one on a special port or a test instance)
	char *coin_filter = iniparser_getstring(ini, "WALLETS:include", NULL);
	strcpy(g_stratum_coin_include, coin_filter ? coin_filter : "");
	coin_filter = iniparser_getstring(ini, "WALLETS:exclude", NULL);
	strcpy(g_stratum_coin_exclude, coin_filter ? coin_filter : "");

	strcpy(g_stratum_algo, iniparser_getstring(ini, "STRATUM:algo", NULL));
	g_stratum_difficulty = iniparser_getdouble(ini, "STRATUM:difficulty", 16);
	g_stratum_min_diff = iniparser_getdouble(ini, "STRATUM:diff_min", g_stratum_difficulty/2);
	g_stratum_max_diff = iniparser_getdouble(ini, "STRATUM:diff_max", g_stratum_difficulty*8192);

	g_stratum_max_cons = iniparser_getint(ini, "STRATUM:max_cons", 5000);
	g_stratum_max_ttf = iniparser_getint(ini, "STRATUM:max_ttf", 0x70000000);
	g_stratum_reconnect = iniparser_getint(ini, "STRATUM:reconnect", true);
	g_stratum_renting = iniparser_getint(ini, "STRATUM:renting", true);
	g_handle_haproxy_ips = iniparser_getint(ini, "STRATUM:haproxy_ips", g_handle_haproxy_ips);
	g_socket_recv_timeout = iniparser_getint(ini, "STRATUM:recv_timeout", 600);

	g_max_shares = iniparser_getint(ini, "STRATUM:max_shares", g_max_shares);
	g_limit_txs_per_block = iniparser_getint(ini, "STRATUM:max_txs_per_block", 0);

	g_debuglog_client = iniparser_getint(ini, "DEBUGLOG:client", false);
	g_debuglog_hash = iniparser_getint(ini, "DEBUGLOG:hash", false);
	g_debuglog_socket = iniparser_getint(ini, "DEBUGLOG:socket", false);
	g_debuglog_rpc = iniparser_getint(ini, "DEBUGLOG:rpc", false);
	g_debuglog_list = iniparser_getint(ini, "DEBUGLOG:list", false);
	g_debuglog_remote = iniparser_getint(ini, "DEBUGLOG:remote", false);

	iniparser_freedict(ini);

	g_current_algo = stratum_find_algo(g_stratum_algo);

	if(!g_current_algo) yaamp_error("invalid algo");
	if(!g_current_algo->hash_function) yaamp_error("no hash function");

//	struct rlimit rlim_files = {0x10000, 0x10000};
//	setrlimit(RLIMIT_NOFILE, &rlim_files);

	struct rlimit rlim_threads = {0x8000, 0x8000};
	setrlimit(RLIMIT_NPROC, &rlim_threads);

	stratumlogdate("starting stratum for %s on %s:%d\n",
		g_current_algo->name, g_tcp_server, g_tcp_port);

	// ntime should not be changed by miners for these algos
	g_allow_rolltime = strcmp(g_stratum_algo,"x11evo");
	g_allow_rolltime = g_allow_rolltime && strcmp(g_stratum_algo,"timetravel");
	g_allow_rolltime = g_allow_rolltime && strcmp(g_stratum_algo,"bitcore");
	g_allow_rolltime = g_allow_rolltime && strcmp(g_stratum_algo,"exosis");
	if (!g_allow_rolltime)
		stratumlog("note: time roll disallowed for %s algo\n", g_current_algo->name);

	g_db = db_connect();
	if(!g_db) yaamp_error("Cant connect database");

//	db_query(g_db, "update mining set stratumids='loading'");

	yaamp_create_mutex(&g_db_mutex);
	yaamp_create_mutex(&g_nonce1_mutex);
	yaamp_create_mutex(&g_job_create_mutex);

	YAAMP_DB *db = db_connect();
	if(!db) yaamp_error("Cant connect database");

	db_register_stratum(db);
	db_update_algos(db);
	db_update_coinds(db);

	sleep(2);
	job_init();

//	job_signal();

	////////////////////////////////////////////////

	pthread_t thread1;
	pthread_create(&thread1, NULL, monitor_thread, NULL);

	pthread_t thread2;
	pthread_create(&thread2, NULL, stratum_thread, NULL);

	sleep(20);

	while(!g_exiting)
	{
		db_register_stratum(db);
		db_update_workers(db);
		db_update_algos(db);
		db_update_coinds(db);

		if(g_stratum_renting)
		{
			db_update_renters(db);
			db_update_remotes(db);
		}

		share_write(db);
		share_prune(db);

		block_prune(db);
		submit_prune(db);

		sleep(1);
		job_signal();

		////////////////////////////////////

//		source_prune();

		object_prune(&g_list_coind, coind_delete);
		object_prune(&g_list_remote, remote_delete);
		object_prune(&g_list_job, job_delete);
		object_prune(&g_list_client, client_delete);
		object_prune(&g_list_block, block_delete);
		object_prune(&g_list_worker, worker_delete);
		object_prune(&g_list_share, share_delete);
		object_prune(&g_list_submit, submit_delete);

		if (!g_exiting) sleep(20);
	}

	stratumlog("closing database...\n");
	db_close(db);

	pthread_join(thread2, NULL);
	db_close(g_db); // client threads (called by stratum one)

	closelogs();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void *monitor_thread(void *p)
{
	while(!g_exiting)
	{
		sleep(120);

		if(g_last_broadcasted + YAAMP_MAXJOBDELAY < time(NULL))
		{
			g_exiting = true;
			stratumlogdate("%s dead lock, exiting...\n", g_stratum_algo);
			exit(1);
		}

		if(g_max_shares && g_shares_counter) {

			if((g_shares_counter - g_shares_log) > 10000) {
				stratumlogdate("%s %luK shares...\n", g_stratum_algo, (g_shares_counter/1000u));
				g_shares_log = g_shares_counter;
			}

			if(g_shares_counter > g_max_shares) {
				g_exiting = true;
				stratumlogdate("%s need a restart (%lu shares), exiting...\n", g_stratum_algo, (unsigned long) g_max_shares);
				exit(1);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void *stratum_thread(void *p)
{
	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock <= 0) yaamp_error("socket");

	int optval = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	struct sockaddr_in serv;

	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(g_tcp_port);

	int res = bind(listen_sock, (struct sockaddr*)&serv, sizeof(serv));
	if(res < 0) yaamp_error("bind");

	res = listen(listen_sock, 4096);
	if(res < 0) yaamp_error("listen");

	/////////////////////////////////////////////////////////////////////////

	int failcount = 0;
	while(!g_exiting)
	{
		int sock = accept(listen_sock, NULL, NULL);
		if(sock <= 0)
		{
			int error = errno;
			stratumlog("%s socket accept() error %d\n", g_stratum_algo, error);
			failcount++;
			usleep(50000);
			if (error == 24 && failcount > 5) {
				g_exiting = true; // happen when max open files is reached (see ulimit)
				stratumlogdate("%s too much socket failure, exiting...\n", g_stratum_algo);
				exit(error);
			}
			continue;
		}

		failcount = 0;
		pthread_t thread;
		int res = pthread_create(&thread, NULL, client_thread, (void *)(long)sock);
		if(res != 0)
		{
			int error = errno;
			close(sock);
			g_exiting = true;
			stratumlog("%s pthread_create error %d %d\n", g_stratum_algo, res, error);
		}

		pthread_detach(thread);
	}
}

