//
// Created by mwo on 15/06/18.
//

#include "../src/MicroCore.h"
#include "../src/YourMoneroRequests.h"
#include "../src/MysqlPing.h"

//#include "chaingen.h"
//#include "chaingen_tests_list.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "../src/ThreadRAII.h"


namespace
{


using json = nlohmann::json;
using namespace std;
using namespace mysqlpp;
using namespace cryptonote;
using namespace epee::string_tools;
using namespace std::chrono_literals;

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;

#define TX_FROM_HEX(hex_string)                                                 \
    transaction tx;                                                             \
    crypto::hash tx_hash;                                                       \
    crypto::hash tx_prefix_hash;                                                \
    ASSERT_TRUE(xmreg::hex_to_tx(hex_string, tx, tx_hash, tx_prefix_hash));     \
    string tx_hash_str = pod_to_hex(tx_hash);                                   \
    string tx_prefix_hash_str = pod_to_hex(tx_prefix_hash);

#define ACC_FROM_HEX(hex_address)                                               \
         xmreg::XmrAccount acc;                                                 \
         ASSERT_TRUE(this->xmr_accounts->select(hex_address, acc));


#define TX_AND_ACC_FROM_HEX(hex_tx, hex_address)                                \
         TX_FROM_HEX(hex_tx);                                                   \
         ACC_FROM_HEX(hex_address);


json
readin_config()
{
    // read in confing json file and get test db info

    std::string config_json_path{"../config/config.json"};

    // check if config-file provided exist
    if (!boost::filesystem::exists(config_json_path))
    {
        std::cerr << "Config file " << config_json_path
                  << " does not exist\n";

        return {};
    }

    json config_json;

    try
    {
        // try reading and parsing json config file provided
        std::ifstream i(config_json_path);
        i >> config_json;

        return config_json["database_test"];
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error reading confing file "
                  << config_json_path << ": "
                  << e.what() << '\n';
        return {};
    }

    return {};
}

shared_ptr<xmreg::MySqlConnector>
make_connection()
{
    json db_config = readin_config();

    if (db_config.empty())
    {
        cerr << "Cant readin_config()";
        return nullptr;
    }

    xmreg::MySqlConnector::url = db_config["url"];
    xmreg::MySqlConnector::port = db_config["port"];
    xmreg::MySqlConnector::username = db_config["user"];
    xmreg::MySqlConnector::password = db_config["password"];
    xmreg::MySqlConnector::dbname = db_config["dbname"];

    return make_shared<xmreg::MySqlConnector>();
}

TEST(MYSQL_CONNECTION, CantConnect)
{
    // we did specify wrong mysql details so this should throw.

    xmreg::MySqlConnector::url = "127.0.0.1";
    xmreg::MySqlConnector::port = 3306;
    xmreg::MySqlConnector::username = "wrong_user";
    xmreg::MySqlConnector::password = "wrong_pass";
    xmreg::MySqlConnector::dbname = "wrong_name";

    try
    {
        auto xmr_accounts = std::make_shared<xmreg::MySqlAccounts>(nullptr);
    }
    catch(...) {
        EXPECT_TRUE(true);
        return;
    }

    FAIL() << "Should have thrown exception";
}


TEST(MYSQL_CONNECTION, CanConnect)
{
    // we did specify wrong mysql details so this should throw.

    json db_config = readin_config();

    if (db_config.empty())
        FAIL() << "Cant readin_config()";

    xmreg::MySqlConnector::url = db_config["url"];
    xmreg::MySqlConnector::port = db_config["port"];
    xmreg::MySqlConnector::username = db_config["user"];
    xmreg::MySqlConnector::password = db_config["password"];
    xmreg::MySqlConnector::dbname = db_config["dbname"];


    try
    {
        auto xmr_accounts = std::make_shared<xmreg::MySqlAccounts>(nullptr);

        // try to connect again
        // it should not perform the connection again, bust just return true;
        EXPECT_TRUE(xmr_accounts->get_connection()->connect());
    }
    catch(std::exception const& e)
    {
        FAIL();
    }

    EXPECT_TRUE(true);
}


/**
* Fixture that connects to openmonero_test database
* and repopulates it with known data for each test.
*/
class MYSQL_TEST : public ::testing::Test
{
public:

    static void
    SetUpTestCase()
    {

        db_config = readin_config();

        if (db_config.empty())
            FAIL() << "Cant readin_config()";

        xmreg::MySqlConnector::url = db_config["url"];
        xmreg::MySqlConnector::port = db_config["port"];
        xmreg::MySqlConnector::username = db_config["user"];
        xmreg::MySqlConnector::password = db_config["password"];
        xmreg::MySqlConnector::dbname = db_config["dbname"];

        db_data = xmreg::read("../sql/openmonero_test.sql");
    }

protected:

    virtual bool
    connect(std::shared_ptr<xmreg::CurrentBlockchainStatus> _bc_status)
    {
        try
        {
            auto conn = std::make_shared<xmreg::MySqlConnector>(
                    new mysqlpp::MultiStatementsOption(true));

            // MySqlAccounts will try connecting to the mysql database
            xmr_accounts = std::make_shared<xmreg::MySqlAccounts>(_bc_status, conn);
        }
        catch (std::exception const &e)
        {
            std::cerr << e.what() << '\n';
            return false;
        }

        return xmr_accounts->get_connection()->get_connection().connected();
    }


    virtual void
    initDatabase()
    {
        mysqlpp::Query query = xmr_accounts
                ->get_connection()->get_connection().query(db_data);

        query.parse();

        try
        {
            ASSERT_TRUE(query.exec());

            while(query.more_results())
                query.store_next();
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << '\n';
            throw e;
        }
    }


    virtual void
    SetUp()
    {
        current_bc_status = nullptr;
        connect(current_bc_status);
        initDatabase();
    }

    virtual void
    TearDown()
    {
        xmr_accounts->disconnect();
    }

    static std::string db_data;
    static json db_config;
    std::shared_ptr<xmreg::MySqlAccounts> xmr_accounts;
    std::shared_ptr<xmreg::CurrentBlockchainStatus> current_bc_status;
};

std::string MYSQL_TEST::db_data;
json MYSQL_TEST::db_config;


TEST_F(MYSQL_TEST, Connection)
{
    EXPECT_TRUE(xmr_accounts != nullptr);
    EXPECT_TRUE(xmr_accounts->get_connection()->get_connection().connected());
    EXPECT_TRUE(xmr_accounts->get_connection()->ping());
}


TEST_F(MYSQL_TEST, Disconnection)
{
    xmr_accounts->disconnect();

    EXPECT_FALSE(xmr_accounts->get_connection()->get_connection().connected());
    EXPECT_FALSE(xmr_accounts->get_connection()->ping());
}

string addr_57H_hex {"57Hx8QpLUSMjhgoCNkvJ2Ch91mVyxcffESCprnRPrtbphMCv8iGUEfCUJxrpUWUeWrS9vPWnFrnMmTwnFpSKJrSKNuaXc5q"};
// viewkey: 9595c2445cdd4c88d78f0af41ebdf52f68ae2e3597b9e7b99bc3d62e300df806



TEST_F(MYSQL_TEST, GetAccount)
{
    ACC_FROM_HEX(addr_57H_hex);

    EXPECT_EQ(acc.id.data, 129);
    EXPECT_EQ(acc.scanned_block_height, 101610);
    EXPECT_EQ(acc.viewkey_hash, "1acf92d12101afe2ce7392169a38d2d547bd042373148eaaab323a3b5185a9ba");

    // try get data when disconnected
    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->select(addr_57H_hex, acc));
}


TEST_F(MYSQL_TEST, UpdateAccount)
{
    ACC_FROM_HEX(addr_57H_hex);

    // make copy of the orginal account so that we can update it easly
    auto updated_account = acc;

    updated_account.scanned_block_height = 555;
    updated_account.scanned_block_timestamp = DateTime(static_cast<time_t>(5555555));

    EXPECT_TRUE(xmr_accounts->update(acc, updated_account));

    // fetch the account data and check if it was updated
    xmreg::XmrAccount account2;

    ASSERT_TRUE(xmr_accounts->select(addr_57H_hex, account2));

    EXPECT_EQ(account2.id.data, acc.id.data);
    EXPECT_EQ(account2.scanned_block_height, 555);
    EXPECT_EQ(account2.scanned_block_timestamp, 5555555);

    // try ding same but when disconnected
    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->update(acc, updated_account));
}

TEST_F(MYSQL_TEST, InsertAndGetAccount)
{

    uint64_t mock_current_blockchain_height = 452145;
    uint64_t mock_current_blockchain_timestamp = 1529302789;

    DateTime blk_timestamp_mysql_format
            = mysqlpp::DateTime(static_cast<time_t>(mock_current_blockchain_timestamp));

    // address to insert
    string xmr_addr {"4AKNvHrLG6KKtKYRaSPWtSZSnAcaBZ2UzeJg7guFNFK46EN93FnBDS5eiFgxH87Sb7ZcSFxMyBhhgKBJiG5kKBBmCY5tbgw"};
    string view_key {"f359631075708155cc3d92a32b75a7d02a5dcf27756707b47a2b31b21c389501"};
    string view_key_hash {"cdd3ae89cbdae1d14b178c7e7c6ba380630556cb9892bd24eb61a9a517e478cd"};


    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(xmreg::XmrAccount());

    xmreg::XmrAccount new_account(mysqlpp::null,
                                  xmr_addr,
                                  view_key_hash,
                                  mock_current_blockchain_height, /* for scanned_block_height */
                                  blk_timestamp_mysql_format,
                                  mock_current_blockchain_height);

    int64_t acc_id = xmr_accounts->insert(new_account);

    EXPECT_EQ(acc_id, expected_primary_id);

    xmreg::XmrAccount acc;

    bool is_success = xmr_accounts->select(xmr_addr, acc);

    EXPECT_EQ(acc.id.data, expected_primary_id);
    EXPECT_EQ(acc.scanned_block_height, mock_current_blockchain_height);
    EXPECT_EQ(acc.scanned_block_timestamp, mock_current_blockchain_timestamp);
    EXPECT_EQ(acc.viewkey_hash, view_key_hash);

    // now try inserting same account. it should fail
    acc_id = xmr_accounts->insert(new_account);

    EXPECT_EQ(acc_id, 0);

    // try doing same but when disconnected
    xmr_accounts->disconnect();

    acc_id = xmr_accounts->insert(new_account);
    EXPECT_EQ(acc_id, 0);
}

TEST_F(MYSQL_TEST, GetNextPrimaryIdFailure)
{
    xmr_accounts->disconnect();

    EXPECT_EQ(xmr_accounts->get_next_primary_id(xmreg::XmrAccount()), 0);
}

// stagenet tx: 4b40cfb2fdce2cd57a834a380901d55d70aba29dad13ac6c4dc82a895f439ecf
const string tx_4b40_hex {"0200010200089832f68b01b2a601a21ec01da83ffe0139a71678019703665175d1e067d20c738a8634aeaad6a7bc493a4b5a641b4962fa020002fb1d79f31791c9553c6dc1c066cde2361385021a5c33e7101d69a557cc3c34a1000292b1073ac7c2fdd851955e7fb76cbc7de4e812db745b5e795445594ea1c7d2a721010a9e82db48149442ac50cff52a39fda95f90b9683098b02e413a52b4d1f05f1c0180f3a3f93aaac0d8887ce542c01e49afbaf1f5ac8e1e4c8882011dd83fa33d1c9efa5f210d39e17c153a6a5825da329dcb31cd1acae98961d55038fb22e78fb93b95e6820a6904ff5b7bbfcf0cd1f6b9e9dd56f41062c7f5f43d9e1b3d69724eeca4bc3404a41ff0669f7f8ae725da8bbc2f8d0411605d2c3e959e3099414ffce1654418020d6ee74bb25042045e955b966e060a9f65b9cfccfbdcb4ee3101019a51f3a41372414c90383c9b84fb4ac690047ea67d70c917192abeb1fdf6c618aeafc732ae9a3e6c6f7fb274240ca04d24035a1d55b3fd1060dc3f4e64f9816868f36d2a0aeb76b64169bc9525d7d70b077c8b998b1204c6108e2917af893b0de24f4adb043eee433b096db8ef730de6e339a84f966b80b239821f350e2a8b10817fc19508da4f82bcb90fcf84cd47929ab1ba90bde5d252767736a4b23d26063cc6b2120dc636b2d4e3e7b3f6d7448882e01c10cf7bdd2117710dc49a59b49cf99212800093da22a6ab5bf693a80d4a662f35259f088a91a26ac261980c537238daa7a40477871f7481973edbcc86fa4885a4061d5c889d0986b4c0cb394165d4039081069f61cff822c7a227474d1609dbf05909f738803834b9e1b5ee68dcb825fcd906d880684e5fa684c8cb02495530fcaeeb35a405345f352ef3c8cf276c096d4303ee14cc0a48533afd1bcdf32dcfb25db4c45a19b10926dff72ace2c2a1d0759090acff99ad06e3f81c4d759a9f74d47fff9cb61d3784aa0eb57a250eec8d14800b85026f5b112740e2e8c257541bdfa4ea1f166b9d930e9705fa1d3f3c2be1e0fb242394d9230c7c457fc8422a6f919a06df5c52d954fa1bfdb57a0a2778d35020ef07490fc3c4f6e99ceaef97fcb3da049898944f3291b96a4f9ce491654340414565db2e0c6bd13eab933194b32336c09c478689cc4a7160b63ecb59937b2028e1649744c6ea69db01aed72fb8b39519cb28d764f158789cc539e01fd5f8f098597c8977e6d2b22c37b289c95dca13bece9e28f741ae2384ead8f0d6df7c30f311210523cb0b7e8c1173aee311362282f3c7d06ae2153969d073bec14ff3605d193bfe829e110df39b894fc4a8eb462d357098688748d579c85f9dc9e9cd20229c9865bc54ae8338433247c28875249e37d4d9b9ccbad611ce5948f1c7ea00fccdc41a9fbe942539a44111f160c9b1dc2653ecae0e4c30bada58d9164fa4c021b60fc0e2068510ef8ec56f7c8e036f2eb31ae0871c9a6aff7b8ad6a86054a06daf06393871f41859f51165f09d22fedd8a2326099a803491dbff5027f72540ed6981f505d1162d86c1ec716758bfbf4e1bfdbbe628b9340942a328209c86b0ec71981c90b407b146cbf065cfd8b0a1c2528aaf007c505a83779dacf8cb7500577a1f8f4a2c9a833d08b254c0c5edbbe87c9eaf266627d23bf518373dceba4099c13c6c468b99c16d56ea5eec12620867fdeb01be66bb0248a7ab7dd6434aa042392c4482e8694292bfc638f1a344d35c9cbfe121a25d2302db4db9688285905fcef9a892593840fe2ddebf0e059e341b2e7f107ee6a19fe386e3f9ea0da2801c5e9fe1b9c4b21155f6f2194ef36f2383097f9cf7ee0994873cfd92d098a33006547ef67bbeb2c1386fcbeec6f0b974532f2ac6e474b26683a37ed3919ec3108b578d3a552c32a362c0bea8bfe35c481f014721af9503a0663d95f5aa91b8802c32f60dbaa65978ad114a2fd45901a07fb4dded61d19e30c8c11e4efbdc1180a02d08d05a646bd8a4d160203da8d490ae0a0b4e631280401d635eb78e0416408c208436dcd653c11a75f365e2d012e0e69f0d4fe2fb2997726dad29468109001ed7c65464ba734e4b35f2ac85a4437fcafe0cdb6128e634970ba420e2e3296080636520e9fbf453e07cdd215117a2eeffed5a0ea0003761f5eb4c35aebf87b063f59d0571e149dc9d0c143a59415a7fe950d34b5024087145969abdc7f7140079bc25119825dda1bf080150bd40142d9635fd87e5dc691899d2686ccf6c9d106c7cdbf783fbbc83fc0feebe8abee3b458e7417a385215f1f89a2a89bef3b870c7a30a6cf4920ddd6eb50439c95c1f38fec867a41e628415ed142b7014991fd0818e8cf725b7075498e773790ccc882db0eb22453f0c9931debfb8d3adb09370f30e62b5fe94ffa7e55ff053488ad48f6e244ed6f043fae8f2225a024800c690ce15dd8017457d23c834b276d079d4a141f22db8fed4ea8f1e378fb741a2da30c09abc817ef0ad589428f966b8bdf92fb32fefa67f425ef1c58a2d4c881e8cc0156519256b87736ed84691b585687971c01a23da37ab76726602a01e771d9820add53f62168d51c7ec12ede030b61155f12e7c8d5573985a9e0884bcc41e0b50aa066b3337f2ab8e3b1031a1c843700413ef03d3734536547fc636055f5d15b0f804b3d25f90e4aaf199ee666d90199d0781526bb2040144b5a638411c1067d0bc5e3ef1911c5223fb0f5d0ce6387702b6957f9466b1a8be6879b8c7190ec1e0662cda6005a363f5d3d4585e6d9d15349be797353179b0a713b925dddfbd05609430601339bf31226cce63c763d887364aa6f1d640892f2a074338b741b7c3303e62c6fc16f544ceeed251dbaed838cbc978eafbf1f837543876f611d64d354049936220af27edd4834df2740842f033821147619098b45b5819b49cccc772506d01ed8f4ee1877c3368e3863f4e60422ea8918758ebd8989b0e26041a906db0ade4365e7d322dad4f40b489b53fed41ed2d421c1571c7311d0357a511eff490193afe446f183baa147e12dcac9827391ef233396dce9465ab6d1e073a128cd062fa829dc3011e72d05f96956976120c973da24b874aa7dab15e6ae58272efc0ba97355b22d5ec283957b2dec2926bfc2f66059c0146ca91b53b0ffc03f3bf402c99ada7e18d713bf57351efe369ea204354343d3b080e8a386931ed541b7e3059f2bf1736b4903a25f0bba187f5553768586202d0cec388dabaaeae9b7ade100733139f77682ffab54409e14605ec50f5648da42702caa1d27bafc7c40c3b30b9ab2a54810e1e4584701b58b3bed1a4d820bb13308e9d5485c011cbb1cafd0021c0e888147666a3c00a59000f9195d6d9e423e11d7c4eb69d71541c7ad943601aaf1f7c6b5c0e058cbc989baa7f3a1927031e2219b398e2483a6b77a5d6e7e029c2ba36bdb882927cc06adc89b312bad1c5e72f9a27b48fc3a2c8ce431882c034a345c2692a323dc87380d62a1afdb73fa47d9b5ac0e495138e0dc7ee57835079ee2e9fae9b4e1148ef7df42e1fe213b99f573ca222430771088e05266db80092bf53d23c800b8f15b036883cfe58fac0682aca8d58d5ef2cb0c0bbf3c39560a347d51d3d1444b98b175af1abe341ece49c69b7022e61274a3d75f81871d780aef74495569cf97c7f4144f43a87b90cefdb6ca3e4ce8b12b168120499847000e67389d13f8cc190762f0c02ab0230e45ab1d53d59f665fdbe73c342169bede097c85f596595cce0d8d40a02ff89f81b64d7126c6bb73c3ff9739dc5af0444f01ab1818cc5f2c6c548873fde9095ca9efc2bb3f4c818fcb873ef23007a1253704ad561b872f774b6cf8dae848f98cb0af3d4b3c45acebccae094edc02b5250a0e259f8470ee88c12fc16014b5403fd3dbe29895aa411af2f1292ff20aa2fcf9038ee4453a51448cc7ff4a076d8c635619711ec70e8c43926f7d0df8f89ea156069bb64117f5be57b731dd3d573721e966216e98b71464342d95e712dfed27f50ecb291728f205428d335f3f4cd3e4657c2dfd66cceee48576f64d921e521f6d0d67df90d553e4c6751cdca6e425f1994fc9f1676a261f831ce5b0238c1a2ec10c6c94c5d22951c47b01c7e43b2e2199e0af412bfc025dc5d2d6ed6d154f154f0f9c7351c25d184ea619fdca59820448ee49a46e63ef871ebcb23af8abc58e9708bdb984519ae76303e661f0c00b44c22b6ec9bf95830fb047bc38106aad060607c6e1c4998b0cf07ba5b293cc96621a3151a67c7272e38f2e62f69528d1d19b060eb8c6f165d95dc13aa2e28b2844dc63bb3e24ac06eed20050251b698dd3f103e2f309501092c1b537656af79a0bbb2f398a2b0d8d7c976308db90dde5cc2b0a7ee510e902624a7c19f1169b6eb84773e131b6dade39030bb22555fa8ed9c904422269debded88e1c51b4294a458a080b78d4c76d72a49cfa988e96e3c3f900a86dcaf5953907318e9cf30b95fac95724df8982387a13f4f92cfa21226a7be0cf3d414afc4a7ff632bf803ff6061fdda12571de6c85477562da2e7cfdf70f9029210fd211239c5242002798598c278145b9501d3c41c12f998a7c82a6a8cdf06adf87910e52f43c79993edb24bf691d52a6ea175150cba134eb3fce090c3e401900e2671b4e5995408f22d9a7fa615301ac6153b69718b7e0079e3b31d3690075967bc173f6e716c2c30ee24863b96df0b1f7ec9c1fd7e7ec7d019bf4273ad0459d3c182ef8603f6b83d702f545cef5b1604f16b02657c94a3cc18323bca2708a8105bca731db97385576e84d78cd3f02e18144f2861bfe43590f5b203679007b199f0033dc73acc50950d2ef9f7678131f562599bdb4e91b97e673e30e7290eee0d5758dc3c1bd11c07df261222e095d182201524c271ee877a7afcefb40400197c1a153e4741edf9eb5602e856247f6845aadad9a3f3c1a3947099fc14890b02129d3382955faad6d4acc3186cb9749300d8ea9b32ec95ac0a8f11515e430b49b12829fa4e4e2e2754fd43313c544cb5b3521c3bdfc9fdfcfa9c35a4e79103a5c4f1b3b5e150f0f95951a4f9f7e43d6f2734b7ce212a1f14d70b2131c30009351dc1caeeac4c546240df2180dfb49dd0c780222d76caa4769fbe03fc826c055a409f05286a61f760ff281e095574472db33b803543f471f79bc711feb4a9052a4236308c714a2555b909047dee9421e3a9d04d4e7785077f71541a46084d0c72c83528bd367b260bf4baec560cab216f6a5ab82164bff19e6b0532005db300feb20a02e1eea6d9d31b4e2b0415295b9fe9c16650f25588ea94f0472328ac03bfd43876076e94c08506673db0b2fadcc91e1dfeb2439c9863e3063bdb833507bad8d987160ec288207c20a0df18b4c9b1b00b1d0feda3505c72e3c63f72c20ec41c2e4e377dce092671ea2f2ba7cb7b291d1ae230f8b26ad3b9ac0fbcf2c602f808c5729f1234dec272e59929673e32b3b4effeeb6fa74b33f211f70b80210670f0866ff75fc199e1ebfdaafee3468a818e1eb7fed5dae34520bf3e1b0c2105afce4636a482be6c9b51a4d27a170d22c6da8ae6324d6ea81f2bed486aab0f01b3eca20a60e739ebfab2cb6a374fa7287768b9635242ce1ae5dbef3879922c06817f3ecf8a08bb64f00c04a820a718748c714661d2f90b90113926a242c7bf092e54b99799098f8d4a10b52117f9b1cc044133a47eaf8b235d89db626407280766fe232f52ba22bc7c19113d43ffc4bfeff57b0dd811db81760403a11221c2056e9ded4f7e88f277158dc94ed1135d9196e61cc80e259381a7f3bc1135eca40fb91f963d6c38bf83d7b9b8dd275a146491a7fb0cb69539c3659f5360ede7170b49c57ea2d4ec258a8792cd2e7746c8955b79e9ac1e5ce0c0498952c3b91616011e98ebae173c7b74ca004c0fb2000b990a7a776f89b12204d02b3362778a67026e448e3c546bae75889435cda08570c72a52d7a30524185c21b5774c9727220408d5f57c713fd33aa2747ec5d61e7852a94db2b8b5d2ebbf90fd3a12b490830e7df75affa5d202f352b4e73128776b08a2fb415d1a354a3dc69c4845faedc007bb1856af1d47839959f012c44c0294505177938c6b4ba98939efcc69aa87fa029b68a2308e53d79dc013b557e19ebfda4bc93b4101c3596cf137c68cc7746702b53a6e2a2d557aacf8e7ef7df513bd85da45c1811529e4ab5e0bdc601f73b000338ca3699d1935b1a4d5a7e9fd36bcbbd9963d3a264c09d11fa76d1bdaa3480bc6230f56760cd25a47e0a384f1f3e921575e24f2cf9ab10d144822fab4e5210c83ca06824acbc8c4acdfe709cfa250185d935b38c4dc5d7e9b9bcd37a61cef3768a2961ed44c9ba5706b9b95e4fd24627fe34ca79483a2a04a483b5a69c297618dd52d9f107ceded42a02818d55f5a322df5d8c1a85591ef6a001357ecd8a25289e619626154939d5c02a3e1b82987411e648257958259d93b220ddc24aeaaabb6286f67ba87cb18bf546cd5008def5780f9af7d7b817fa2d30f9b2f4c667f1e63f9494e32945b4ca1fee45ef4d8fb7448550f5ded771ae79ec0ba48fb0a2b7ab4d7500e24ce807834e708c7afb9197cebba8755db2f06869b317374158c8f2d31c3d35ec57b0408331f5aa18da7bd620360b83b1ffbc47019ab09a0fa18d12511e22449a0ccfd24cccbd2edebb149f5b59bffe31bcd6ac5132d2c7bb804414165d17770f4eb8b8253d1cd393c4dc3d9216a538f31a4db9864157d0bdb900581dd3d85f45f36124c8f4ecf876d09cc626108b0769cab79357e6fe34ec1aeb94007a5416e8dcb13b0143db8da0fae95284b872c562f9f272e8d29eaaf09c8a4a3b1bd72892bb3ad3a4c4462969ccf431a40f3213940fa530c1adbb2d595e8ef94aee36de8ed09087d769cb8ad966404d7f689b60e020e1a977fbe70e57a31c096814d3f1b5e924d55ef79259599dadf4ff376d5d120c2d5aad9e14a92b8b656fe915a294aca796cbe6b15d4b0dec70aff9988a00514582afbb519f64ccc0f989c5fddfa9b2f50b37653e3aed60213339abfc6d0d58808d0b677e98248e0db2e64ac23b371ca2f9d2662a7c359d57ad770dc7ac4fd6f5652fd4e8dc66f8254969a37fbbfb4b2197ec864c18e1cf515f6b2646cf8fe5ed55427807a7aac9016cbe811c86f0b973916d5123dfb8e8aded328a464139ee92fd22c69d60fce6906b2174938a88b62fe96a1c8f977330c548ac479622b2c89db22099d5b1d1931c1bd15c49ad655d1371bcbed1c42734b3ffdcccf40b00b9be3e7cf6f5b8da625a450d1cd489360140022a468fed9be5bf8eeca7ad220ebca7e4de8ccc9d5ca2a8112d07a0014f8491bffa6968f6a70789c22a49ccaca63f196c063a29e69f03399cb7ff7f50bc572bb0902bf43d9c741202779c6e41b3e32c3082ef3a212450f0eabbf61e943d7d1a7dfd7a53c229e1c6e22a6d1893367b2b803ed23825888f799d73cc6471d16157f70c226fa39dcca4a08bb6fb3a512ff06b797f869007b39691f2cea01360fc5eb0925715f36cc246135279ee360522090e88ac7993cbd9581333c2a872a5755dd241af1e0fe04ee5857d16494222a32e4d9a1d07890492ce8320f164f0980ea133024f370e914fdb5b1ed02f174d0fda91c4a0f5cecb369745e5d7e4bf0fd4f0a2f55f5cdd5e5d20dd5076391c78e6f97e7c930f6bdacf40bd5a2edd56d3b68ed4027bba853f0342192a379999127dcd27c73fa21e06d37a6b6b5bc39e8d7c2189621f38e74a1fdad551586919d4193a3ea024160460ba1ecc2c9104e43a23bd24baeeb3adc43543d904d98c0560a246f7f21adc0fd21205dd4f2bbb3f4a9d16c4ecda3f558548caaece8dcb08e97d8f086e51d559505128753ccb7d2dffe54e3f7d5b63bfc9472105529f5c1ad027da671eaaf3d6045fe0b556e236a17d15b7d6a916a6ba91bf2e75045fcab0dda68803371c08b576563aa779a3c3f7b739c5c3e2cf6ece351e790633bbfd7055e50e93f6436aa7c4dccc986c6f0603ad20e9d810e1296b89a3270dac70d7d6cabe6bce51085895ddf4c2f6a8c63ce53c14fed78b3f89e30452a9478bd1195a9fb990c4f27c2d18f7bb54bd459549d6908255eac8437ca24a51a7b050714cd034961e851fb90efcb89a81e3212a0bef071ab6102906a9c0735f0d56f496011c39c25bb03e146f98121020f58ebc06da4537dd79e48b016561392b8047fd4047d9c5c0afe1babd8f32c6292fc36b1e79c0ae14c42ac2fee2cb108d40c6d91f264a3046485a3adaee2e23b9a34bd6f5b8c73667b7983ed1dfecbf927203d9afce3066bc7fa9ca4ae8d234192d637740f088d43b0296017bba1530c726c1c337c00ab288b97376a6a97ffb05e6596b8955c22c3501e373cbdbc54dfb1bf60367840c895d2cdca3513a1701c10e1d11e79d80bc098cfd9ea04d4296f4da2bd398f52ab76bea76730d82e76836aed863644fbe5c8a4f22aa7552de4d53e71d42e51b4f59851025cf3e4c1a8b57c2e026194f104b0865354683b09a13765451ce31198cc5e20ca54c1b7142bfedf03a7763d3eb0cc4a0ff71049a4556ab962caa0b3b8dd8bc78eb227e27280f789b728b416c2df52fb672628d7822e7757415003d1975634dff7ea0ec3bdb2bee4bca1de669fe68c3349a590cc9640c976b8d3580acb183ad2a9b9923e3ce7f2679d1656d921f4615c48466b28a8fcf309215eb1d6546084a7c53d837a59fe0e5fad510bc7691c29880284b696f387d5cd2214c08ed00f931a412f747354f98682f6c19e4c42e07d8a75729b8578a46a9d3a998d94b21a0c9fb235d18d9c172162341ea2c0533b267b692b670f6eca96e4087d4dbed18bd2cc7a7466527266210919dcaa8ae1e19549ce14a5e511e3d1f59836b4e09e59f016cdbae1b715030b9662bb7060f05ad0bbee62373d074a9b53f483732777c612cfd4c275fff73d8a501bdf7a0c2eb8494deb2195a59b6b0f99c50a8e99069d56ec104aca17f0ebcc880f76c6f285ce3d03c2f0be55d836f45f188c841c691d720f2d2365c0e23709e370efbc8eeb942bbdcbef23aeb118631187239b9bbcd70de0175e98b684ee6023b96702ebbf1ae3f5ca8a9dca6498afd0252c44930b1c90f9d3ba43f4aa6a75ea0191ed61e6e210ef71a2a7389c8c8f947ff668d85e2564bb07510f9a8ba1c58674a1910d0c4eafbe00574a8308b19df2f8b474ed5f81b080a1c9a07edb216f236b0219c5e4d5f24aedcb8f1807c55bc508f2cbc9a3e30458a0ca4e4ddf138897220d757c876c50f1fd14173e6358d338c922a4438809d606bc9d91816a634331ee025f65760f6650fc63c9c8a5326f0e1f0fb1916425dc108e684ff10d621b7ce10fc6633c7432db26bd713b532a7d9788db88c42f5c84c8f576b83b49b2bad9fb02c5a0740aac474154a6205270d67406e4b5d80af40896d696a0faebca9c910609d32b03f1b23ac722b1fc1801e5d0fe4778abd4f2662d025fe7905e9b38b3e0057d37c0bc65f9a6982fb2d3db32382860c2371ff463c9c81fb991703b548c9e0f777ac72ceec2149405126f9dcc5f18cba19012ace4f8e936e4324f30eb366d07726f143546efaaaa8309e027325b19e4f2b34481fe8edba09cfd67e48e201c0c251bf9218d0d2db8286692e1c558c59abf1b563ef17828830423164e642873080a20476073dfe969afbf47b04f69b44364de220a7212dde70ed18e5d50688e070190894c8bd3b3f06cd38280ec280849316bba88b6925bda2ba3531a8ff6100f6ec2124282045555a2131281abf07d2b36f2aaa4c60c98fbc5a72e716f66b00353c282443c1ce8bd23974be486e30269eb8e4583e62f1b9ed15d1bd180f5190e5b72ec9729b8d2c32285776e8a84aa32150bc08a12e0b09c3055181052f65c0d09844ed2bf6ec9158f648196acc5c5e740f6c7093436943ba181e60b0b525b0615db23ac58b1b67acbf1a0f59ea1a9f7ec9769e7ea0519aeb3ba1e8d783a2a05fe8a34bcd88749249e0d7ba8a0bcd1cdcaa7d836d2534585eecf38a1dcd5360d292e8a9c7c1c25c90a8fc4eefa3f22392809fcf6764c06ddb169b0058b505a08f7c38f4f8e8c12c888729c23dd9ae33df30917120150bd59720ce5dc169eed0edee1b0f9ecba4ad43ff6ee0e1103c784bcf1e4068a348111f0f4ed7397c8720fdb777c4206f8b70da082a2f8d2e92220229fa6c55402119861e671e8c9f31807b52d3380a8bcb1d29bc9795989fdaceb185839dc16cf8b115b3e0ba96eb4ae0b29762bd84ac30f8afe03f0ac7140515f28aaeedb50e98e51ee16236aa8410c0065c3813512229c2d0a96de8ed252074715386e5a5eba1b93d77cf344c9466e05ccfd6892160efd60a40af3f94ac81625acac363ccea77665eb6bfe33a69c570af107fd693722c0e9439c36ddd7d3a4a2edf496cafead3455cd23b69190fcd201c1db6315c8c03592c86a4ad3c17515eb30ebcdc42ed85099ad99aca3af87d601137cc4c899dd7b860bbcecb1e804d4ae784dedd01b3ed1e04d74e86160244c08619121bea73d7afd858287683958bde5319f5d1020700f52ffbe564a6e8e9109c1f8423ba825c221be5e9be8cb1d6b74827770c365c024b8989b18a45c612b0263d0a7c3cc19bc6c048c5a60fb228de2f43196a8802dc44eede70cff7d84cc0af37dc7829830e7d888136543b070d9a99472c1f70255bf70ddc9c0299fbeb007d92096ab2018d68144def74c223cf1d845a2c5b850613307ffe27eb2497ce80cde0d7c8243f3ac86f0459e3dbdfdae11a0168209e86bb90d0b47a5b91efa28093fe2ca554772cace53c08fac18d003be5d79afa78e7888057dd473761335bf007b0dc9cb3d79122c67fe5dfb6b3715a9412ec7b5b4b9dd95733d86a63442530f6a21f0b32249b40584cf3b3aacce380d5d3eae24f42fec00e6e9f73813526a07152cce67f8e46d89b3f6c74fb564d43a1ca94e9f04a3d406a635656ea020c4088f6102da9534922ba27d53281adf30b7336616060884c31c7f83491fe3ba5104341d2897549feb54172bf1ca72e6d95263e025c4698b1530076aca0089e59103e55ed49b46c356ca5458f2bc6789c2ca6b8062545dd830c5f71768f82ebaab03be78c77035c67a567689ddc3cc8726ee7811af7ee8c06e157d4c87a909c5840fa19359e07395b820876d40d80a7b4aff426b8e4e2803ab1fadd1b630e6c1db0de6809988ca5a56233d318f94204a126a605942d0628e7cba88ec227d4018db057b45a3ffff61be5e26c713a4362e7515a4e0b3d7f4132b6f5543e76c22a28d0eb4359c2aa0ac26651ac69385350cd5f308c84db05cc0ce991d31d04d18c6890d79826561017d38bb2ae636aca2eec453436d4510a4d52d571865ee4aea36c0070d167ce0035fb2aff4a7c450b60e0fc92d6e7405bb7b299d14537027b260340f1576bde7c27fdfcdd4bcb51c96e2272ebccc93fc834f3328668f27130dfae60976e6e17ce6cb4a4a2a108e58c539e90172c0109141883e0bb9dacc2b7df1810f84454f4e185538749a986ee94b34c2587d36813f06496b5c78a85a9fa9bc300e22d8f8055afae6504cf54ee99dfcefad61c4f314f1a9d831cffd498e066a9d0c389276cf18bf590091e78cefc8c657ac60e22ee8febc0acbccfbb6d0bc756d018fd57533905c01d33f684be06b6e8ac5d3164c2473f2d2bbc2cc6f33ae3d980ced83c2f9691090dde73ae3d94ccf5a02a32e5866f9457321b2c0c6b2932d100d288012fca3b8caf82080a7b254395bac134277699f0e91ccbe429df8504ae00d334e471b530642a542d470280b1157e9edd978be004bdbe1855148969f1b7607656fbeaa3dbf98da4103fee661c049f1a8cf0f722d2ca188a18614048514b90eb3988115b87b3b120363defec35214322fa1478530525cad835fbc075145ec089aa2a8efc02bfc11466e5355d955dcbf88fb74aabfdbaad82ca18ca8bc80a703169aaba1f8b1de6d036e5be656f8739f04f0172f91d0a3c3cff6d0e9f265dd0a0bdd5f9e954ee4534646adcbf4d157a90ec734dc4a0c13eb728135b29daaba02d5b2ac38b390502f181e756c69590f6c6bf2e6430cfa2e6d2f9675a219501b0c6a5006d1ea075a196e039fc9b4460a22ef2645bd98ddb11300c4d76ef0646c04725320885933e206a70c7f069408f5ef3c3bccc195e2f901cdfe81b7f01a870332aff32cf339f1bb2601ba45d062bb0fb1c1f182a55260b19c18aa65205fb409af3b27d5c85be5a508bf735a02853ad7269c63636cc3ccd0ce4935b500aa720737def7452e39b3235d0e115961de40c174063883becbbe895ac13fc6574df7045418ea8a8dbcf512172efcd5dbfb6688d949da143dcf082d879945901dfbb601406e263f616bed1e969964e000f74e9c8ca04fc8e4f064071daa34121d6b5c0b8886a328eb8b922bd1c4c511ad867f498ad96841ac7fa2280b6ae0be1c439d0dda808cd74cd5aa8698588d622f830a898beeadbda5d0ce5c71d39c479e9a0e0f5a481b848bff2c6668d80d6ef9668531d814d8caf51ab6c18eeb0fd688f0800e4416f24f4094e76cd19e9f083ee00ecf4275ee01e6f9073d4a621e1d98ee040ec72a8bfc1483478ea24dd45ea01ee8a5c516ea50970efb0e3f21bbd5737e6e09ae78488130cdb3e4fb52f54e3f644b45f2c88d35a28a1ec9b009afd3bb8c410bcc09f411680d7181084b8e4a58099512fab865f39673bd81901adccafbcdb409b43808c79551548b10d201cf8922fe5da8c2154d601007980aa7407489fac50cb5a734cde8580749968d53efcf066856d190543ff5e4bd8d02b66346cce24b0d9f33afafa9c899ae7145c1f7b5b0e85090dea52379a0121e6566903078982300fe9d03c7b57335ebfbaf31f52e720ace0c467703c27844084635060815cb3c0b986ed0b406cc389957548f04d70eb2a8e4daf0a49e9a54f707acb0a3c0f81d0780595f9a2e58f618693ce928ff54f2a4a47601c08532959c6aef0915b484fd0ed1eada9fcc2616e181d3de0b5e95c880d866bfff792e5d3e5549b80f705e94095927a1fae3a2038724b5de59a786dc8bc3d3286c5e7c114b9846345c004a6d05491f29f8fe23d6daec6ea5f89de526b0549686f8ea05a950e045fe1af95281097cdf8052039ae20553f0bb12a875b507b3c394515709acf465113c51a361cc0c9d8f621010abdb43e2b96f1b5fe05b507b70e80b9382e3129b0bea448dcec60d78eba7fe0db4bd25d96cb8a8a5faba3d0ee8963fdfbc0cdd7ad16f97c5709103f62e1b3b015ead01cc315291c3c2304b592e104fe8a008d54dc8fc15a7ac39087d63f055e7b87b9312d58cf3bf4858a2cf9a9319339888b680783cb902c9fb0a111ec094ca9f8ad33863bee0d0c243769b5fa8d57e81cf7077e256785fd3c703b4d1dcd47a004d8a0dbdc1ba9a76bb8631b29bd329b2742465b07862fe833308f67aee1667d3a7be0d67d912b997dc6118b9f4dd759dadf1a325f7cc534aa405a1641d10fc313e6fb0e57a5c2e2e603c5dee3896319fd1142ab0894a8904090f64817ace87388361e7a5b0e418a366308e7504d0347daa7daf90213b39c94b0bcfa9dc63d5b87daa690edf897b5c4aaccb614f267071853b7f6f92992fb22f0e42fa32681c44e181783cae74b54682689153d1bf358a8c3eca0eca65ff48780e7e760fdb82b1bdd4169c37b685d35c2f64b9f2a6b009f62324ba6da8bd4a7606d1940c5700a1a57229d82e6989414ae937f60dbf0a275ba10dda13b2c4502a01a5690fc2ea2ac5cdd445570e568a64f7a7871f9694eb3905a1bf0dae9ded1408024ad299d2f2d04bd6cab1e7beea5abc6e3cc7c87226d93f329007fec0099004da76d8ea1c8eedea6664f762a410e97c527ac8189ee310fd3136f5a6d552100b3f0d1572d84249d88a218e5e8b73df39a8a9fe14d900827a857f3aebef534c0c8bd3a42443826ed5c4ca4ba21591f080f6fdd304103cadaccc461638cf23b90cd8ccd1916603a6e9bdc086372e91bf5125e15bd837c0f9a6a691e6da704cf408dc8c88ee168eaba1d057648bb59d04d69c67df87cbd0142a6d5bd87fbf4d2d0f6fb0adb1aa1652dcca6a03788a4296abbb0d688daabcdd1963e7b98954f6be0ab1413978513504b9e5b2038438a1460cc3199068b916a7349f78881395e2fa08317db264572687c8891a3e6f41617d6448bba0bc17c116221d3542dc73f91b0ab490218da61eeae4b62b5f389bbc2d8252c2114db65dbed68a0405e0d153af0a344b8b68fa2f054402fba9c2ec5b6ce536be91b1929fd4ba3d8e20a2d4d7c1007ff87f7df5b4c057ee42c371188b63d9a1e9d1e63cbd296bdbff51c204cbe800c342558408a4434d554af4030855ddd0ef0295417c669615a7635e0d4a3f08024077d4bcfed78faa15c805cadfad0d0bb1f628dbc424a1e7a945e8908438d2015e716c83cdf57f8d6065fa86182c80577213f07437d9bde259a0e54414af410b6516de6eeda634d241486db5966a4f4aeb9d18e92dc9fd9f4fbd9a4a7ae15d0ddfcb8f4f6a5bb544f045217c36345fe43b64aa0c5d08ba1104fd13b57d070806e7d8a83731fee819f5e8f5a892ccdb64fde073476966d0d8ff13123b3d24040db42fbee63630b0a86858cfa4231844bc6176f36e6fcbd94fa014c321b5a05d0dead8501cf0015adc4e933bac66696e6bc92ef1345ffbb823207df4869b5d720e6fbcab98627a54501421da7a00ceb02973fe2daca9f78d41ac28c0bde44b2b0905733b8424df52ffb0e67b2d67e0fccec31f6a5a682dbbe42886c621d2da8c0cee5ec5c48a6f5400ceb2607b9d1094ad665100a84c35c5bb82eeb9ffa0695e0afb40b5e1605d58c8a50dbfb91a961b387f43b263eaacb365bec80b42615e33009ef7dd83787b0740e09ab385c991720b003377d97da4c3fe273bf7d35389980a19182e174a5b1cd8ffc83be319f819b6a5269c96bfc467bf3414d9260bc2e10b9a8fe95b539397ca8fedabe611c63db9130e8183a77bd346ff4520bc502cd323b5d10463dffab6b3278e73e5771ec7d2b103a66650333f58f04119036fbd34c42a021cfd12f2d68b19d1aa373e4bf6621dc2b7c42d0b27ce6fc01199ad5f34e74ec954dc8bd7342e573a58a8880ce28ca83801519e9dd7a3063db98f60bac11045815fd39fdaf1c1890bcb355cee46761fc6bc239c3584f1482054a9e345626f6a1b142fef1970da21a2be3f8e052e4e0f9a0f176099606d6e29dafb5a735fddd81cf462e40c424daadf72ec02f70e2aec1d18b7de4ef9d1ef9e364dea63e4a4080d720b6631268537be3b74c225782608fd2f0bf71c70fb84d88f0bdf2c0649a77e94cfddf710fee9d39e765746e09f3437c665c6c962a9043a798cb564f83e4fb5c99ad93ae89f0856c74a36c0fb7512770c91b83e40cf4960b128cc009e3aba2abf5685ea44453f4a2708f793503f4905423d4cce0a135f923ffc3176868ea7fd2cbb1c7801ecd483024123f61ed919f58f6faab196283417cc575741d348a98a31e9188768115a0eed01b8f47fa9b62a89e821c581cbebeb0069f2c7fa75ec25a8dbbbdabfa130b3443afeb03416671c872bf99ea7f87655304c5270e047a719716554ddd0142a529e6c49b67d41e78bed286e3137698313ab321321866f97f4ea898d23c3a37a941b19702530118fa752a06b7a1e4c61f1f3d9633b73b0c068ecd20e040ce4e0d2632c27dd41faf65144aeb30658a9171dbe7daabd36afddb03e4526e20d201ffd849c42cf3146b0860a0514367a579df4c0fd9dafea9949810711ed7be822d32fc901ae389693fe39e5b28750e510231457e09512d5f97733e99576a421cbf99a51ae3cbd7126423dff294b3b037f3a0aeeaba2ca6a8e1c58fc5b72f1f70415d0fe46c2871e73ab7a4f50a9dfba30c415c1e45af4dd48a7af8b0b0ba49409527e1b2835fe61e43610681e170d73681519efd0ef1775588ed5991b33f69fd378919193ceefe6e99b0fbc55fb2a0db84b054e1e1add6061a3fe18131257ad27e5af6338a7bf79e3497d966ac9906b935e539a4d9d94adc4302561c6725219ca4e6642227c6f331424750ae48189ca4d54f8cef0a328d6e51a0e4073bd745fa07bf77899578c200eac6e66d19245db9d6f8b8437fef9a7afbb69d1a99de13a5dce22c3f4fe2bff36f73e5243b1cee4bc155afa6301f3704c90e5d075fa079d178ee92691ce2152da8765b5f38f82b186d7bce1c853a81c7b8407e0906e88df7b754d58035c5d0536f7d6ade45fb47fd489a5a07ef58e9956e10308a399d2511fdb8f1548c341f7c1eb2ec632a54ea30019d256b595213a869b4502bf1d2806910ef2a31c2eb09fbca65598e1503d911f2ff0cf6c4d906cd421b71ef909d7ad6322a38ecf7d061c212dc1fd5106bbdb726e9007f78c94f2d628096502c5849f8b1c3e79a316fbb6951a39858b333e9d393b3c28d0de43b798c68cd8a245b1e0172c997dd5b8ae89da1154a60ead79f6b2c4151305cd417b05ed13703629de6212edc92322ca841febd3d6d0557d4e26fda4e0f6184ff06bc026f2c7f2b1023a4e30692682ad39ef70201c66bbe1d8bc39cc2f8a9363fee904e6aacc2eca41034791af9be167cf62d1a1c94b9ce32c9415527b43f5837505d93aff050fb8fe6ed47b96eaa81561e4ca29d6a02a04c9671d603576aa492b40c014369c9a9933ee07e3836502a3e7bf5ed99fb35f65e2e0ddc3dd441258cd4d9b161f1dfc53ae5699a3788e169eb585fd91276945dd41363044db68f86aa133349db78f34d54ecb163afe3379edc7dce02238e27d799cb0ba5e1b300235b6e8bc7e450484b07eef5c24954e4df7b75b04e73740e2403b3d37c73138765cbcdc5307c051dcd4466a7e21dbb0c6f3a30e07e89d057e6697c3fe848309d98a050bffb811d158301eb381c3ddc87af73831334db6f788f6d24afbcf83abf9c3a9b1714842f44329d74ff49d6ff5264a1b283ed91a9606919526a7515e105c7481e1468624fca6c6c1db4d389d87e196363421fef88338dcbf683b4540a484fcff7abcd6b5e5864454283c511be98f5fbd78180adf8a168f20aabce0667e3e672cd4de67b1d942538ef557218f1c4b9f45f53b20cf4ae1f001a56c230e363c8df87bb6d78c312be74f5da0b15bb18a6f219b9d52ccc3b1edf85958c492900204e9f13574a88550e2a771e588aff9e61a87d39c5929f6492a41bd289311c7d8740c4a91b6fa0c06432384476f068c0035a57c19c8730e5768bcfbfd9a5d6392f2bb4693990b19faab771bd6cad9e0f7cbec70ba3a9cbec9adcb821e709aa0ad74d921961179e33272ee98eef61d4755607517cc9db96e3bdaecf0c2aba155ebf03abb603c7913d6da5860bccc17ec6f072d2a95f51f41c2bd77e8d8bd997d68b624bc47037e259de8a72188cf1f86f2f36b56c92769998055c5dcfb0a352bbd4753369d3cf1a85ab820557f5f8e3fe76de3e5e6ed3dafcf66b4cd8a82375823d6ac2fdf3f0a10c8c03010d3309452b6a9b8bf4e3a7f47c01748b574d1257f0aa18e6c4732e2e69566101c0ea0b6437d2d64c28902c18351306ce6f1e62e7319e614113b2f426102ec80ca2978d8990943bae86d2077f35019cb83cc4156e12376733876447e0e91ac58000b93344e7409f3eea4afbe6309e23d18c0f1db8b41b6206edcb8c3cea650ce609a62d5647eb8320cc8a40d4ef9b2f900aad835174707f4ec0503ee701d280733d38bbc5e8ea2fa6716bbfa946a9add1b54e3471accb65c9c835cd6912b44dd343a077b0c7e14b89007b15d3192f811bff695929dfd824c1881718287d6e8b3e4a31573dd5b8b9d683e35144ea22ae99eb46fae356d97f6996d01b3353c06bb9b2323fe18f4c2b84d39a3732b38e9bc494cc43e07e64ddfc101fefc1aee9fcbcdc03b1d9120e7fc59633a6ca9f041b32cd43f4f37deaa30ca04542db96cf77cf08805361d04cf373d253ad4727d59968ddb4fa1b33474a6da08c39801a33725631aeef8196795b8c6aa6d56b60706666bc57b22849fd3fa500c27ddced7ea0334f9b92d590696a8270acfa03af69bff0f0b8268d374fa219d0cd224811c9454c83c533bc2719a08aa96be554a0f95fa4ca7c02894032245400b211a9eae98d96052a04fd9d7a8bb382b48468e6d5b0fc861e14ed137ceae560bc0cb0a7dc0af76a97b245966b22597162f4ec6c9f3221a4a69ef43fe19011e0aebcd2d760a352b3cfda10caf77ec902a8ae7bec3700899c9d726dba2df21f103bb703d885b7ca78ac746ca3e99831b4e2f7dd999ec44cc6922bea346618e6001f51f387c2cfaecdc740db9e44ecb4b0da370208ecf3eba1c15ea70bb06d63404025b000aada40e40e1e737966d7729c5084dc8e9c6392dcaec97b4dd6e1b8b004831345902ca2628784ad7ee435e803d31b5275931210b5ea4fcd5132fb8eb07d79fdaa8ce14a5ebbe21d7c2388841207a6920f04c6184fca780803a97930a0f5107450f0e50b530753f8a7efe8c9f298cc00b5b585e49217aa82da904ef33095421011f97c377aaf76f2390c6ba3405754e311e35810701f20c15ad15bd930c08f3224945681dd53e49094d286f89a0cf29bd90feac7a616ba6cb52f47f910a"};


TEST_F(MYSQL_TEST, SelectSingleTx)
{
    TX_AND_ACC_FROM_HEX(tx_4b40_hex, addr_57H_hex);

    xmreg::XmrTransaction mysql_tx;
    xmr_accounts->tx_exists(acc.id.data, tx_hash_str, mysql_tx);

    EXPECT_EQ(mysql_tx.hash       , tx_hash_str);
    EXPECT_EQ(mysql_tx.prefix_hash, tx_prefix_hash_str);
    EXPECT_EQ(mysql_tx.total_received, 0);
    EXPECT_EQ(mysql_tx.total_sent, 100000000000000);
    EXPECT_EQ(mysql_tx.blockchain_tx_id, 93830);
    EXPECT_TRUE(static_cast<int>(mysql_tx.spendable) == 1);
    EXPECT_TRUE(static_cast<int>(mysql_tx.coinbase) == 0);
    EXPECT_TRUE(static_cast<int>(mysql_tx.is_rct) == 1);
    EXPECT_TRUE(static_cast<int>(mysql_tx.rct_type) == 1);
    EXPECT_EQ(mysql_tx.payment_id, string{});
    EXPECT_EQ(mysql_tx.mixin, 8);


    // try doing same but when disconnected
    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, mysql_tx));
}

// existing address
string owner_addr_5Ajfk {"5AjfkEY7RFgNGDYvoRQkncfwHXT6Fh7oJBisqFUX5u96i3ZepxDPocQK29tmAwBDuvKRpskZnfA6N8Ra58qFzA4bSA3QZFp"};
// stagenet wallet: boxes smash cajun fences ouch hamburger heron pulp  initiate hubcaps academy acumen weird pliers powder jive soil tissue skydive bygones nobody sifting novelty ounce sifting
// the list of txs containing our outputs and also when used in key images
// ./xmr2csv --stagenet -m -a 5AjfkEY7RFgNGDYvoRQkncfwHXT6Fh7oJBisqFUX5u96i3ZepxDPocQK29tmAwBDuvKRpskZnfA6N8Ra58qFzA4bSA3QZFp -v 3ee772709dcf834a881c432fc15625d84585e4462d7905c42460dd478a315008 -t 100000  -g 101610

// Txs associates with the stagenet address and viewkey are, based on the xmr2csv csv files

//    Block_no	Tx_hash
//    100938	efa653785fd536ec42283985666612eca961a0bf6a8d56c4c43b1027d173a32c
//    100939	00458128c40886b22d15cbf3c02fcbb1a0860cab654d81230dba216b50fe887d
//    100943	fc619eeccfa0626f4be78cb1002a232e0ae8c8be6826f800341977d23a5a8e1e
//    100943	9ba6fa1c6f0277651e38b9f076a2cf674d92d683beac42ad93db1f3cc429cffe
//    100948	da22b85e51644b7c5df30f65b33f4f00bb58278e8189f9073e0573eb6df1fc1f
//    100949	3fb5d474378431bfa1b01e61965dcd5a62e1753cb7b3064ba7adb98bcfd398cd
//    100964	ac715b386010bd95a506fde0ac0405aa8ad72080b1d7cef257b5b112d9ed84bb
//    100968	9bfe477f2df750dd31b2a3939b32a5017323586a11bad6cd81650cfd2d54113d
//    100985	380ff7683774c989c1fd7348b90cd37b76d68ee6f93bd5239ecc3bb3794030bf
//    100993	8901400e2cac9fa26a47a06d76c8c0ee499ff691df9d22bad2a0183943532040
//    101028	6e1368cd8c48636ee7a327c165fae95496a7098191398c950fe26031246287f1
//    101048	c3004a573547e5cd671c79a6f33f7907e2e6193d4abe47a3c5328f533b3d6cdc
//    101081	fc4b8d5956b30dc4a353b171b4d974697dfc32730778f138a8e7f16c11907691
//    101085	ad3f3d9ff1e2dd2145067950d68d24cbd53db6f9d771f0d275c33fd88e458cf0
//    101553	dcb792bba1da7a9872f9c36772ee8625282b1f144380288fbd8acb968f6de56a
//    101554	c8965d4f54de1e39033b07e88bb20cacaa725a0dc266444e2efde6f624b9245d

TEST_F(MYSQL_TEST, SelectAllTxsForAnAccount)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrTransaction> txs;

    EXPECT_TRUE(xmr_accounts->select(acc.id.data, txs));

    EXPECT_EQ(txs.size(), 16);
    EXPECT_EQ(txs[0].hash    , string{"efa653785fd536ec42283985666612eca961a0bf6a8d56c4c43b1027d173a32c"});
    EXPECT_EQ(txs.back().hash, string{"c8965d4f54de1e39033b07e88bb20cacaa725a0dc266444e2efde6f624b9245d"});


    // try ding same but when disconnected
    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->select(acc.id.data, txs));

}

// stagenet fc4b8d5956b30dc4a353b171b4d974697dfc32730778f138a8e7f16c11907691
string tx_fc4_hex {"020001020007ae8c01df25afec02bd15ba0bb405b20117d3a9c98911f1a4a400d141164449ec5355618c0f5e0daf110dff3ada0c819d02000229f2fd7ca2af272b850887ad29157dca35ff2453c7ea3f972a084de55cdb7fa50002ef172599c1758c46ebf2c79c770c8a54dddcd548b7d2753d6338d25747ae29862101011f34ac725f4832faa17b2a5232cee54ce97bd75b91e6622f4652d92b68c0140180e4abe5e701bcb1b544a76c414d1422b33fce35b4b71459922e1243b2fd0e4483e440486106491c2ecbaad46eb10e88657e8512cd8bd92531144aef6d53ee0d44946c4c0800e7b4486bce9d83d7544f7b41c91de608065ae2161d0711684bda35bfdc673305e32806113437b948f5a2675ef2b855023e318edf836cca8021dbdf8360204b036fda13a11780d40c6bc4539883151dc455bde739a676214ada553981880f22d8db1008d6fa3fca2181c0d4773b3048074f25dc925aa2d7f930b9cc09e38510fba12d161394113f12ac6c0445413d0165a13aa4cd6c55cc3a722d64cafcae6209fda513bc778ce07a2b263938866a1a8d50f44b70a4a1ca8bea6119f109744402c739bae72c3e61a1eb91e762bb89c35d3c61ce699177c047811511c97d80340532893a242d12771207e5a110a417dd13ac8d313c9777050d236f5ca69e70120f0b44bec7777ce71e2b6f9d8bf7df3cfe41610f923179b5d9609ad8cdafb05c0052e66336de3877209b02a50a4858d3019cf35739f71196938d544e81eaa05206f863386c6622f61fee4b24f8584923e0d2a93a0876d26fcfce792150214e570037e5965fdf76c6c6d9ded1197a483999c10eb99693ed2f80fcce3ee99593cc0aba40964b8533ff60214022e66f62905ba226013193a95bdca25358bf911f610da27a07eac4d06909d8e367bdc69a8da0106d13aed43bebaba5df4b35c6d9750210309523e1036fe255df408a8ba30a9a8780076b86371fb4ee65cdb491fe620fa7e57cb715b116ea18f16e5e240b6105e12ea5ee0fac73aeae97e1eb43455704a32fe4ca237f6c84ba78edcb1048c4daeefedb8a6e464fc790ce1bcf5fd654074e5ff5b36b2172e0bdf57a0c72d5bbf1a1bedb1ceb30f6a62adcdf399101cf0f41a85968972c1eb82e65a72d5f23348809f92fa4cff546ef1aaea0ca163fe20bd33e655de3ea6808c96883d40c8625989736a48f83cf873188a14bb17a8d2b0776c942297e6679fd43cdac69adc24ab14ff8612c528fc3ecccbf465953b0dd0fd59389d9c48ef22e2a71a3939fd377b41f6ef681c5cb0236e12927d2734f9808636e85ad5b79464b4ef62483bb25989b41a2b806934cf1f4c9a49b524084880d70e6f28f8815c8bc2391014628d261c77d5dcde5fe5eb874e759a21deb7526082b34351d6c100c17d3b165b7f370fc2f38426ccf039c37e0990949b451e0c3034a2a14009589a0e84a0aaf6479a754b2eae4a95fa4ef5a50b20ac03ec97ac00982ebe4b8e946618189441e0e25bcbf04e058e2906a7d39bb780b5459053a6406a3ffdf9295f1dd95754489ab6b3f2545679ac83c23bda77d79a6936de33f5c0117618ecfb6187ee19e36447575203e52a953cc27aa61c8e3f847de8cee11e802622832d19e1c10442118a7fbbac64b06bb0e582571b68a7300bb23d6bf51bc05e928f61665f231df8e5f52aaff541b528228e5952071e6de61aaf27220bcee036cd82158e84bf97102e8781fecb09c8631c8075366a508fd8f66226c1fae030d1411f7f460411690c3d045dd2e27d1ff018ffa8fb5ff60b8b48db7023fdba7069a9bdef6abc8dbac81ca43faa7ed0005644dbd5eccfe62112614d6e88fcadd0fd9974b96116e6806104587ab2525f19942a717053e29ddb061ad5de7c84ab3012e80ed108a07bc8043813dc344a1e7f3d51fdca76d264c5bb88e6677f8f528065b89edc7a60e42cdab99a0df7b02213a17d0b486b31a07eac84a8f137d575e012b3ffe78178968e62fad7f4ba1f0c9a4764ad2966f68853011f3654d33256102a6c07c96f6f24e2cd8ff9cee472d1a6a55f45b4f3447247b2e131eb86cc79f085a5372ccf515171cb896c2896dbb4492705eef30b7b59a5c655044840ea7d20737968d6e6d9753d911e6eff2bff6df2013d9834ee9948cb5bdc8abe440152b0b3c03383bbb0c079eeb00d263b96d6ea26fc03925e2aea829a07d454982d7a2017699ec3d8a905e0bf444d9696b33a2172485f2657d278aafafac4eee51f9bb00e9378d1e9bbc2267eb8522cd0aef3eb085c91caa80fe2a8f446119c28dd6dd088910952e6735af40a10906e3ff61e7104c854f0f39be2d285475a342dd433b038a239729113d823adc339eec733876525fc7f0b4cc86fe136e919c5e10167703c4191a4b45f014a0e381f5c062c9b813da5936e7168b7f8a6b42fdb9567c8103ee9f3b273ed386e6a3eac4a39fa14527d9868e38a60f9523f2ede898ff9c1302963bfafe769ad670fe83703375e5f79dd8a921f23d3781ada2c8c3bb8ea97c028d5d359c3f04f2c67b1b4edb39a27c5a99f60be1cee63c01a67bd6e46e6970099b25b9b5d02a398cf081adb6bfecac60d6f25d242028d3916f3c67d72ce9510a5a61c32fc7f1ceecab43c8dbb1b4915b85aeafc7f4fcd9f87b3cc8a584e1f70cb18bed117c292671af9979f9081dc067b1209ff24c35e6c6393eb6c404365b0980b94ccef498362d27c6d8eec2d3b7ada754006ab34eecab5dd8a3cb1dfcb1003263d2a77ba3a97c57a276d24327a0880dbdd9cb0e85e390acd6e487ea979c0b220da3dea14e114750bfd94f92e8e61f8d47d40828f84d1a4affbe1b21c129025697c06938038f90a9ca6995008ff45e5a01efeb45d91b7ac8fff241faae3707a1d161ebafd34b1cb930dc802ca821e40dca2c72fb4580b42b37c8714c3064039861633c21c632921f473d0bc4ac3af06fc670442597af7448b0090918711e02cdf2221ff8b7d06cd10250629b8213f635400423ccce4aec1181ead0c8a96405acff4c99119889cd444a5be1b545fd5216a77ce71a20be14fea574878e3e9c01b83e7c26ffdf0fa395c1cdec087040fcf48d8fcb8aea741bae1ea12836bb150a8e7834bb952e52692ef1d130c13169b4ed33bb7917e9f1b0deb40a0e79a74f0b2d21f4ddd728db48be8cc129d74c399ff0f9d2e1317ba9bce4803c21be5cee0ebd5109f3c926ae2490ce52dd0f77d77e5f3f0c98e996216dd6943b9e1aeb6b0d0fd20e5888b3dddc2d67482291d1a664e05b213e0b3d97e15b041dfc887ff30927e630e856a7ee78ca99f570c941630979ba73ec875d8850a56befb73d8cb0027368e6eccbcb57dbb2afaddf2bdf91a2de7822f227827e6ecd1a0d94a40e9303f3838439fedb0d791d5fb6104200d5485db72e2849eaf2251ecff1290465dd0e7b28150f6e2110e8ca0472b51d6c423835438dc55bed1289a69123a617befa0d8c20e2ad522c7e3b465539935ed336e3135782c8bcc7360664396717f83c080e3135ec73e1413b6c7834edb2dde136bb74f84d290ca85bab3b97c1255ae0b10a6a1f29ace12c0d063da259a18b1fbb12fcf5d26162d2df1805ff7b4558112c05e1e2f49fa1d920950ef81784d4b420fe1bf2a6a8a80ae2b0579e3cf65d3c360609fc23c7a5cb2bc9920e1ee06b6df4eeb2f9df25fe2d847764834aed1290c006ef6c2e2e698265463b891e8110e69977ce90f08f4a89b9921abc36de0c0d2608bd400d5d2ebc6231372f3479d50af311aab1cbbd74cf2430e612689674c3d30ef6e9fcab9d24cdad8029742da13729f78c20824a8090b3eff9c304fd531b26079ea28139752b7709e2fc8992adaf2be741e334221b575866c56005a69923b60290471103e5426efa0b1c34032840bc4fe62473f30ebfeb0a29cc821149ece50337d498038f036eacaee7451812480051c441ebc3320aa8b5587eaa6e76050608ed3857f5ab89b054aa99397ffeb18fa40d39c57395f4b76ca54713ac0b890408b65d5e1b75659727fa6d1e28db88f70d95fc8db78db6bd57e0c3bbcf69eceb095865d1e7839b109aa8be0ca24d5c8124fde0a7ca88eb8b9d721bab4be5683803980ced4fc32ada32c38639e57f724d493f420f7f3060eb182637ca4a2bea4800fc04877adb676ac6762a84c2256e68c83a005d631b4ee7cf6262b892f6d4690774bc8a394a6be567f15073b04bf328031a9c4cb8dbb17bd699408ce6df778f093d5066bd54114bb6138440bdc2e973ece822f0a68c610bca9669c096985e6a004482be461028a5e3d9cf6e3527dc5f97f10c0f9dc5be41a844dc6cb3c128dc0b2710d6db763328aab0e0c9a3e4af1ecf81ffa6c90a649a72981cfb5f3519880787bfccd8ee1e17eff1cd7d0d4a5ee873e3c2af4832fedbe85d8c09a16521ae0ad9e262e2683d292c0e89175d2953765dbe71370c85716890b9c7377719ae220406f2f195d7ed87e854bc3f63a0f65f1721a0410092fb61fc51a818335578c405fb685735ef67d2f8a996aa20b80c28d41284b8738307198d80bf50560d2ad109bb174969ed678ce53798f53956a8664a00faf3a03e2a6e1a956a06763546dd0db9be140557637663e68c7349804ccfa8051b41ab978d5aa8124f6f3428b9d3056f3b14338f8eee48e5cec6f2bb86398b53e9fbca44c8bae5ef8b365cb38859090eddaf36864a47e3610faf90897f5ed148a7d1396787be149cc6170d1d9abb04215f6afe5469e9ffee959071faf207d01d6e65e133fc234c371372f7652971090148a41d6081d72e53e0a7b73065cc8b5aa7345d57cc316b3df9eca9496ea50ee38a1bcac724830ab9d5dac0d1c5366f744e1489a238657f9a41a34c2593bf01a1e6dc80801222650839a50acf8d8b5be641d6b9cb70ea4e9fe8d14e791c540603df3679a13d009ef0ed12dc2d2326baf517ba85ecc63a4d41ded53b93ed51074482b67cde9e444c239be64f8b845c1fb3045dcb626a46dbf823496d17be51088a02228f9f0dc049ebb8ce5b71bfbb6106e2a0fce51cd1494cd1d27ad4879b00ad0ceec91d2e89aa4b285b0f4e8fc465fd936fc4c1ffcbd114b863c50414680ef647e196bed4e333bad666fbe33a0f473e2c80db7b8d2884ccc1b07f07f7440124346a04c5421ea262db2078a549c26d82b1383409b58479c51559bd01db32053cd645346933a3cf0078fcd10abc58d88cdd0048810d921e3ed8a65d17babe041a2953a2b6e173d828801c22441f814f2717dc101d8c5c8e942ef6d23f79ed060efdb0cf868546c4db69cd55d0627a4c087763252bb829215b4255d07f7e380ec02ffb2e7f37bfaedce49ef44f3cf173bad080f60891b17f18b5aee27b13e6024f1480d6295b624501310caeebc08e3e6e876e9181e8216f71e50856246b790b8f3ff847db3a2b7b359dbb8c20469c03d57314964c069c038601288da4bc1703d008716fd2425176580ce37d5ab8f725c8a16d5c401ea8218c6edcf20228520979eacfb5196d198b18d6b99596836070c8e7d8a72e6218670a6b673136e80f064a9aa535c5093a9b6851f1557cbd646e6d215d0204fd9d5a8848225c67d0b701d90737d8264430110dd1e5c1ed0a354d84d6586098e0c6fd616d7986e09c1803a76388630d148e35bb4b84226a85c7d6b12f309db33d7ab933ddf740cc0d1a0f976c8ec1f80dd538d8b70d4ca2af970858d4afef260e72a63a3df07f1c6d520710fee4ec08c9109dd29780053d9bdef83c85e678162fe759925de5a0b459f00d3e42de303400b177558b95b34b503482630111d6de6b2cc74a6b6585dfbf4b058e7af908d47f301fed17880f790ad807609d61c16f63d9123aa81dcfbc012009fc08b1bc07adc932ebf93a84c7e368bf4e879e0f5c953db4253ada65689bb50b39bd902cf3672f9c3b0744ed856a57128c563212b4536c415c36927b18406d00127c588e5151561d22d135569e4e0fd8420f7e7425a019e00be136e1f737ed0a6ed753bb63cc1f0b251bf61316ad5eda6ffa2b7aea4a7c3062257e9d8abba907b3a9e528931977c12708b0cd2bc5ed879bd1b077751828521b5c70463c22440bd189f65f1cf168ea7f998c1e5cd722bf7d7954eee8588172752c35eb86c17e0503185c41123ff4745a118df52bc5e749798c3dfa63bb42a4ce0cb63b7b0d2e0f85c714a3c0f33ca5443b8d83f324e1fc878a9e123a6347fce84ad2d189205d046211f34907bc2506c1fb4032f218f25771bbb0e90df3084af1c5aa258603e3078f3380003b70c4ab072f664debdf80cb77cc11c49c0dd09f51359f5a810eaa0936155ca7ce040de895dc9f755b09758046f3906b5db0c2cfa1401b6e088faf39d170f7a6c0dd809c3e9a31ab2a5641846dd4df2ec51c99570e1f332fe375c8337dd4e8114d1c3cff70e2ea80022dafef5c579588a53123b278090dab5f9c7f74186fee0d46f899a5fe581e7216ea8af99dda74a8f086c770d2e9a7a0f6bcc068343b5a3441481289e3a387d98ee886922cb99466a62b6afc96f006117f94c3c136baca7c7b935bdc6effbd9017a323c54a6acc051aff1be9a94b71dc8dccc8affc638731396ff93fec006e75002c9c1453df70e4d6cf7a0130c849a1d6c186d0cfb954705fa0fa7cdecc06411d6328de0c434f5993fbe955cee4380644acef0bc4a71bed6e53897a3c3675974b6e2130d93b327eb77aa70c0288f48046835fd19114b31be27dec9b78589236cb5a94a894a5a974ba138f53da94558ba34fa452f37b0f5613c14c0ba9071810329255d01db399c29150274932a153220ed29010d4c6956928f48d16ad04bacc8cc3e3f78cc899863f9d7b235102594c05e9c07953c2a2a83482599e104310ce9e253d9146fff36ec60327980cb7d429c658a18de83d076f42341ed6ef4811bdfa1306d394454e059b159c51976c1942a404703a8785f1a3ee3f01b37caa91f3845d73a6098857ed3e0a7fa7d991c4ae62d888281a3f4344660b6c3f62b837409d21146dd00f41a8d830bda3877e86f64190d47201f00e06826b42c4c6e43c94ac6cae8c9e7859fe17a2558938146b99e1b183468bb44bc54752283b673b4a86ef86b1636ed456e17ca2360253798538eae8d131ccf437faab4a149b08a21db2a81008634076ad7116dab798ccd64b8a43c4fe937fd1c960f6d1c081c8985ffc8b1bd27eb798d62962ae001d3f18d180054db16ed1ad68307638cab9a35aeb7ce0e3ab5fcc880ee7ada2a70cae82b6f68a6e79adf7b1bd1e85752c3d99ed74e4b0c8619531ad215ec56e49f9e09c24325cdd45c28bfe124983fb26d6b99749c0d5e5ae03373586cbf2da50e712d1b8b694ec14208199407a14b87f2c54876dbf7fa72e0dc925549b125f5ea3a6e3ff82437fb42bfbf7f9741dea383b75b12ac2e07c14de0022782bb760377bc528b4e620430c684080ce0aa253f878f06ad4a08cfc0278257516703c90444bed6db8c4d7fd8e76298321c1ae4bad22fb16684b60d63c6556dc2abfaa70408391696075e33871eb49575a07b851db808b5eb834fb1f05757ea58cd804581be5cda5062c9f8d0c47e9478f00cb9ad85f1f561b4e794bc501f9e04392bdd9da08a3010e2bca92c326c4a27f6e56447f7cf743f68c5e77cff5e7e06a027d407b19a279f7c8e7195f2cb50f18bad985b251e0f0240453ba5db5c2b27b66c164d39337575a06ec8267ce6b70763c21525cb68b242a83d223756648c3ba3d2ec5772e85c77037f9494f8be16caa1f4ad835861276e9b4eac7f70a8802e2a882b6b6938a2e76eafece2432c2689806edfb5631f98e51fa9519e02b5cbce04c958d7792070096456821f20b1e98bef6e8ee87e057e6a04f8ff11e6a2f38a693a4de4977d76098465fa26cc0184fb80863448a5bac21cc6685834ebbb6d58fb52b04461867116fe0f388af15f55413fd703ab3909f55a92ce0bd31c7debd17dca5c9d4f5ff17ac7bbb4d6061b6a9f38892986cc7bacc30ee94daf4f4e7d3325dfdf79cbaba8a3c3c713f06e2cc7ae8e03b3e1441f3c170a0fcbd725aac52a3e6693ed79d4fe89e6b1fcb67ca46c5ba76d431dd686302a267d18ff863ef6e4ba18c5b6dca782ab5e5d6f28ada91c77f3b3d94dcf540ac04ee23267f536b07fd18ad65976f6693973de249c3cd2050de2accc6666ce685eac40fe13346af619b440e4c4a234c0731e1b7af8b9a612ab63e2e172136c0027cb001279ad6c605d18a1a588c649ca16fdfa6ec870346d1514ee16e4430bdd6d6fd3ac879413a9bc77a8b9a7ca2a42e4c8ddcb9823f93a47f84417f74f4333a1d283407a39fd2577cda17c564e2bb2cf5960bd33ec7d13497151befc85b24f06918c9f7e251c8b95594062ed3ce7c575c59511033eb8b9781bc57c9709ee9887ded209618607fd72db69097c7a79c992b1fe1564e1f72355987739598a6a0bfd8d9b8bd1c02ec6e17128cfe13ea9944c7d5d42e78da8c3f25970344891c0e29765eb1ca72dcdff9b0e76e72abff31d1bd82e058b2a0f1191ed3a46e36a7919b1f8763d99a9cc711a4eadd09e2cc64d9d4499c9b49429c72294b80226e777022e292e45d6403ce1dd21a56e5118caeed4a5630602dbfe4b58d12ef4398f48b3c7deb9a68426eea30bd847b762c1ecda891290abfba0e461faddb3c9c01ae5c5d6e7e266cdb7d5737ea8e5c79e47ed08dce7db33225778ea9c971a43dcc37905fc70f007b47f165ea7216a9c2020b580e2af2c04ba272a5744277766b2736d61422fbc1c621cc2b6e69b417b3717e443423df97345eeb4eff65466f06011dd121fe5f66ddae66836d1f28518a0dfef5f16cdd2b73fe29c1f170ef8fb2f611f8f5c13e2d22198fe0f29b7c6c01537e36034f552bebd89600790126d194f6c74c83dbd8de2181b2418cd22b101820578b290596f9025ec60f8ff7d09486bbb2942b50db7fa6f8bb4642baa33875dc30b52c3a020d2a18cd6b895e31b1aba7c438f9d9baba35b2d80fb8d13b63c3e73d9ac6c897fcf1a045bba2b5309402a9f8f1989a6bab16f6f300a87c1654f69212b963cf08285ca2f3595b6c7a57b1ceb2757e1a80b45fe575ef0a9a18c25dda64d466f51eac525b15e40bba2b3706229bf08acd24a9178eeb28b24a91354b7f076cffe9e288de634c01b584466a04cc701f6ff4c6d5144c6a68faefb95e1a0fd25110f20c159528f2acb14c66a50006b2badcb46453f13df5b2322970e2584f528154d4058bc634039bd51cbed4bc1b8f873f05d3ea431abe03b079115ae0959b98c98325f02f3508d91ba4d777a775ae55ca9ca7c8f1a31078a26f5f0e4173c756be4a5169bcb2026540ce9c0785af2cf7ca641299d37a5b0b046b25247873e6f04989b966043509fbfb711447aa4e0ac5d730249cb1c63e043914b66ed9e03559a56747ed23870a84a05b16341d296c6ff7edc458860735507af7f85ce629feb26102d5c00b8c04c6db6bc2c171c0fa8753a525d7293175e1b142185f100f5396241a9167ae930b30139bffec87bd120d34f2df5a273fcb7befe78ec1f60a3ecf6e0ceaeef7910b47f1fa7044d448e3a73e226c32ed62907d8bc8bdad3074d1f156d375bbd95604cd94e0984d53b054298c3235cc07cf648c60750f05eb2abbb8800ffdde91cc0871e2f25c08e631cffdb01cdcbfe44c892909f819d2167d70193e0b28ded65d035d28002d0ac9de20ab829e11703f297332d7188889acfbe515e48efa3b1d8f05373e91ec65988f28a2a9572b7552a8163ac2b7b9b9cc90cfde44334bc8c99002dfb8f054d4f0cfb9b9f8bfbcc473edcc11d1c80a29aea4daee47179751aa6b0e91e589c33948e7e889eea43ae37d371a928e8250a6b711f76edaa95645a92d04405649ad44a82f351d73f72585715a99183b807400cb26d0af88febdefb6c103c35b29390c1e19ec4682980c84bd042d544ef4568639318169c3651fec8c630832e3373e64963564ee1dd8a4c8150ef17e5d61b5c0d17f6214a90454b274ea0e37250bf27ee66dc9510734bbc8c668e5814df76c94814c8ab1c1f4bd2fd5500e07f2ca1c3eb7ca9e27a2043705e4a009f07cd0a929dbcdc9713b70e66f87f00521a610c9878e8c14f89e5ed7de5f11568c3fbbd923b35cc12071902cd7e8340fa6aa3dcfe7e84320f5bdb210b15ce3d2afb60d12eb540db54642bce50352ee0e93fdc9df5d81508f8d0955a436f32712ca54a9ae0b7534b31baabd2e58fdd300ea28ec4d5ea61e556525966bedeb942a5650b2a603dd2cc3071a1dc7922a680d47197c1dbc67ec1c793d0dc9c336e3c2c1b4a26de349662513cc91858bfef10b795316943e8b2abac232b8c9cffa5d61d59b0a1122eacc00583ed1dc75430301f87fc714fba0a5fce3c4dd9a9e9c96847f576e4cbe4ff014ead0efbf4c715706cf01e5eecf972c9921ee416de3699e4b72593ea560d11e2373e42e732d99290890dcc663e413492a6522d2804ac62c7d55a20d46d4d0d3fad0d867dd039a2b0377020835475ef61c20752c04b5f38a17c272e2db7342ecae367fc1f216487c07883720fcc282dfc08efc7935eb1b0a0bf42ac298dd5c9a916b166126929bf2032b2de60eb23fe5b622271a1af5a5a3ec9dfdfba4591df5a8fe829e0b40f1d20f9f37d9294b7219f39789bda30e398edd526daca8a354b8a0749e6ba9f037db0cf04226a85c875e092dd9dd8352efe3b9d41035cbb37fcb5c8c7dced9152720052b535feaaf82a853c6ba28b0f39d90dc81439cb8c9e106560a2318cf94b9660756300b88906202bf953b2e72dac4360c29dae468eb6748127aa37c2563f76608ebc23b0d5a69544314f089da63f14cb3bc805f731499e7f0341b1439019d2000448b94e3acb7d13d205dcfe66ee8c356131d6a9ba7c18cb08999cb2ddf3ac707aa543174c1d0cd027eaae9ef589cefd2af56f3a73fa8e9183a545a4d396a3401b940be736e9a7386811372f10d7d51a4bd161e74e97c452f6c7f6dc2a54b6b041a6213d945f10bc5407879cf2a3dc8ecb3438930e6a33f59562359c8769df103a66549b0789c43d5aa6f04385ca4bc67130d11db877161f557ea416c9f7f8c02d8972e41bd218b365a0c4a79db062a3bee49e255c603d2e4f330391d2a2265047e2335f15368c25c86444c1e3dedee7f4f66ed63f7d01820f000dd7a558e260c3c22b1a8c3ecc5e5ced98ecc0ee175f2f05779da527a4d1c628cf7f8e8dea6085d4f8e69627517bab68e530b338bff26c1cd68af620113bf96a686f74830350b3f183986559de35d7644ff871f765d482d2049b19a7080c10e9c1512b0fa900321b024eb55ed1c6c4b431b0f771aacdc45ec96d12af28dc43e170bbbd4d6120fb3cba0e2463ee872c41368b56ebfc213fd9b08b92f98d33d2419776db4b3a50a39e08f6a3e90959d451bb98c95e2373b785739e9bc1bc87e2e62d06ca404f60da6df3128bd7c0d80c22536e57bfb8a20f2549e38f318d84e64ecb958bafe4700c2162b4051d9695318bc746a92e3862d715a10543046238f84f86ee8c486d300d8583daef2e594e91cb13b1437812459cd34a3486cf6aad8174615a939b0860c5ca413e50c15cd47483947df6ccf8934ff1d45e8f03860cf5a12d9270ceaa9059b61994359c62d9472331b2030ed9c7c747f0ed129471c6fb763a95b87a2af0079d2c5b275fe3f30b3e26384369edf9bdffa7ae5648bace5b94fc4fc87cd510b09b869a9e57219d1d3716edcf83c1c1054f8b26dda785109d0f4d1be846a25034dd1314d745ef5a7bba4c06ab4f181572c5f17d1e90ebacefd01f6c19111ee0b6cdee074c65bd6aa9b13fef8a57d75a86e5da69dab954a622e5f43f895cc190187ffc9d010cb949a1165eda9f9c9cba1d7d4d081971ea47ea22aa08674ab4f000ceece4979ca1edb0063d11e09bf84c4d089f3c2ae68dd5bc445e0f6ba8def00271da812e422d021a504c1e406d17e37f94a4679a7a3fb66f5c748b262d8270d2fc2ce859ee39b13ff59283c8921342799ffb550310a8f5af86af5eb2e0d190293c1019281468b75f35af733f8e47f43a093df24c70dabd94bd0e63a9e21f4064e4bb9bf251d56d7ad5d422abb02341843a451b9b2f027abefbc5ec73c2d430064ef10fa880fe38015487a24d292b44fa6f23fd5f57c59bce461e6ed201a870da151c5f5bb2837fd64d4cf3075146fe28322cded089e12c64e5219a25da7cf00c2b2b3cd353a3669fab4e2574f543d3837fae7dca2c9341834c94afa7a284d0882daf71467f6c638a826974d3f54c5bcb5d2662e7277e6af4ee679724dea390f7f99bb6d4c9cbd207f10a624b7ff66f2cb0d520dcdadbfd1877bdb83d7bf750c9e1238c9f3f21ca8c3a354513d00bd9d95d7ef6ba0587cd8037934361bfdcc0294ceccbc5bc1cfef1a0effd871efc6d85c1a6fa9023e41959eefaadc4d54c40481f6a8aa491262f2001efc331e1fabba4d91b7626a7b10e3a9e98e10ffa35b0019783457887adeb6ddcdcb4cbadfd5ffda7fd8fe634ba5ced289a11f95347c0ca428669dd3b51e001283d0ee263172c6e722be2952154392509e9335c1593507e1c2aa443b3e6ae8eb602306b1ef1d527aa3e6e1e7e19cbc316772903409570bf2258d010ea94c21c209e6bbd795c23086a0abb19012a689d43ff73dbce3000149ec2267100db98375c398b000b5d4ad86894707fa6ec83f21349bef5e70fb00f003a96e4dcb8e000ce16967f8c18b3ebd62ae23ff1908d2c01ac1b63c322e016725745e16fdc30cfef0748f4b7612967d26816a8a633be70c8689a278fd0c0c511e56e6e91757c40f7eeb450227d3ddb90a4eb40d3782ce5caf79d0559baf0755bd00e2cdc671c1c07116aa66233d7c803a8045115a93e92fc6c6c22bbb600c86c857296b59e06124acdc642d5c57182024583dc1800210f21b847e9d7e3c0c2614dd2884e9058b0e9744d4edf6d274150703be53d062b423ce80bc58437407c07c446f85b836a804eebda32dd63f2cfedfe762b63edfa0dc02c4e155deb900f0f96142972529bc70bc68059427b22cb4a04b87991524e6b3147e8fa5a01d065332f007436f7cd956d98429d5c17e15754c05c7eae038daf436bfb4119f4208a3dfb7cd122b80743437e6f77bae85475412719b6fa42e71424c0319163b7a0a8fdba521ec8fae7279b6951fb44919a68882695e940fed98a067aae6279cd10837dfdfbe267902d4fa67f38f26dd23b391763396a334d793cdb24657fd04fa02fc78901990b8b6af09cf461c73e7bf140efa4674632200f607f5157ba939c0060d1a39e859a7e20292b4d641e974b315c598e9bce49c564b77b1ecde559aa80d980ffc3edd14dbedc3c3e4556b9f073db126de077e633339955968907743ce0ad2ee4fafd8d9b4fc130bf2b8b048820b35018204b8dc62d46c7a43c67ffd0d0591b28708d3c1ce23f2e946795c2fff01d974807b6dcb1dd5a928d3fbe58c4e0c2f520e1a1639dd3ac7e5c6ea6c2d64e99cf9dd3fab65cfbce5ed1a43bdd87f080cefdd4681f781f620dd1f933b7e4a9bffad96b03d4afe45a6e42e5acb20600ab5cc65c01ede3a7bd9ae1fe015b7e31dd38dd1f945aa2da7e94886cadf1da90c8a79e6e47d26c4df2bff8319d3d9fd40e3af9c1c5a8ab1569320ebff50a09a0e7299805a8d8981819592aec08701ac2d77ec4e50eed22e141663dcf300a8550038b832fead1cb3b1b66620a183c98f8e96aa1359500a5322e5d63d11abbb860541d4af5046bdca9f2f606047b71852fd2c423888e46d8241d79efdd9eceb3f0b42908fae082da8f29955bfd196ed747556109fb4703a5c49785e3ed3e44d3409c402215db467080cac1563818a7fe4395b218f62c801bc478bcb4356c1a492028f35787d2879ea9ba86ffecd7f3bb9869c85cd2b348b401f2a767b8cf5d4d90df7b177ad6f122004365d67f73b864a607184e62381aced9fa7b91176f2c3ad081332bceba788d11cc974b5dbfbff1278129c139c3394d6a2df43459c23a02601b1321f76f8d2ffc42fa6da63b2e12c2072cc638c240fcb24091d654180b6d200fde8f3ed934152b2e7f2765296a0c61bae3768189462844f6c0c52ec6ab7570b52a5378aa1b3d96cd96d976c023e8895d27adbe396730fba8b5f2a2496aadb0a2c3ed2fb988bd9939a21f6b5bf9e6e3d6ac6486011a223d9ea8cdbfd1221f20c7e4423054d68dba7d7eaa414fafd94615aa00ba904ceab2a0ad2fc8246cc600c3d90947b4699545312ef1afb60c4d4d0b0249773cf070c9db13cb0e73db65f0f41c5d745ddc8666fa16c357bb86fd32947961df3151a99c4180a39e29325cb0edc5311286ffcc6cbb3a9facbef8da06813188424000c9deb9dc68c9aed58d50ddeb0fdc4f948b551a3f6583b72ac7ea88be6cab786d0b721b14334377f250703047f43f550d666d34fb6225433680161d86c81b93046b18c94118e3f4933490b728bf5af8f99ff0c57eea742531187f2c35ecb0538c65f258f947fe89ddac6045ba2233ef0f3ec80abe950d73061bb6c180ab809aa9f48dc576874a79010a00b6ae106c74e30d02178e1854e224175c0b1cb8761edf04b96fa9f6bf232084e0c7b3cfc4d5123a3cf5f3898be31536fe4fa84a582a649f7732d84f2d6324a1f0703d07f4d0bb98e72b487ff4a4d7cf7dd4a0acec7be626103f32f00f7a898200f0eeb243e21b58e88fd8eaf5e2165c9f8267a75f0dd71ae4fff9d1a895b90f70edfeeaa445d092a66f21f09a9999031132416642bc31c875d8ec9dfd65a20020a407b359ddf99dddfccfa38de349fa59c3840655aeb15585bc3a40fab3df9700119a3c93fc587841aa53923ce4349b48a39f26a646a5ce4faeb63583dcf002a0cc989a3e0c583fe8cd4ac7c14a2a37be61d399187f65b680b3f384ef78805640ba7bc31a5394ce8a37792fd791346e5233e9a812f7784665ff573ef8f11b33405c5c07723c6fa6dc2b923c52a57f6f9f11c544be4545c28be12c1107fda202109cbb2eb0a6f149d030c0b24a3c2ddaf488a73edaacc872ca46e9675543932601406903329c91c155a965509c9e3cc8703ff20aba4c6d596d667e6bde72c63d668cb14844d969d8b548c42d579b097b7c7468c6c62627565df8ac4180b245e965202c25078be2d69c25187a2cd385bfdd4e18c0498d6099fb827d70d169b125220f1540f799c8feb305e75cec4b0d50da70d3321fe1d940976fa2a34a8c01ad6c5873091c1a1224e82e05797493d7f87878c70c58c99e3bac1dfa7684b5f9c6e05432a3bcb630338560114ff3c2ee30274d68b7db72eccbeb87c08dbbcf84ae8415df545d277d73210a1ccbcdea083e512e879be5fcf25480090a322701606a7e117698d455a3837d96eddc737f31e4aae638abfe7207de415cf70e0bdbfd4aefd89d1e236c6b3e14b192adb244d93efa22c5c3330b81dc7b2ac5d5f50d847d23dc305566fd276d94156b60ed1542ad9467839412f23afd46689652344801ea43bb48c4145aea745ede6b6c41fd410a6edde9afe1b25269668b708846c3121748fa9ffdcc615c9828c6c60f03ce335faebb2f1a9aec105fcb17fe4271f9f7ac14dddf2bf2a694f131d0ebe906bb87a522db38d930ff9c41705e24bd63447b06bef095af6060a283dd4a724f9377a1cd087126391aad0de1fc5ae266394dff0b52726c09097b462a142df1129229c6f9d460ee3e34f0a55e272198acafe3a6c833a5465ec3daadf18e214f693ae528ecf4ef3888439efee0eab0d23bc4c43f96ca2b14cc68932e9e87969f58e6748a7fa7fd0d639666588817abbb31ffd567d40447bd0445cce41b62c1df7477ddb7e7add80fd6e65570432704dbc6e92069536588326e4f1ae08b6744c8cc9ae9b0af3a8c1ea30842610c8682af247d6328ad17b636dca0b5ffe66cd69a3b0e162a0747589a5ea20c2fc0e08bea581977e9c640ff4930a02ffe9f410158f35052a4e68e7f8818ad822e38d93c41684f490f9da16cc15192b88cafcac42c523e866b19e8526923aff6c8eb06fea22ca23230b1e7a4b7652dff9b3fdaa470e40f13e3d99b10b2f77cbdda311fd15ae57bf20d7396e2b294795c5a382e17867a102f4cd4f9148b31fc190b69037c62294b5a23e9560db8369954e5e0e6e6442f5e076b81f8550d23b4d1debc027c134f36fe209b40fe8edd23cd0ed1459fd190605235679af6a4ea2a909448da9f75999c42ea00324305f2cde7325b5c5b1b74acc03d5a35ced016f9bc09582e687ea6e5e05c430ce23df7b5f9d8f0a4285deee9abcd8f3feb259df2ff1d893722db2dc832da27777f3714420f0914afc09284aa73d84542a77fac5c50958904d77c606f251bd1800547e9a830f60cdc2a5d1e6521e1bfa966f1d282efdfd3f2b4b880986624be6e05db82990be1b61ebd1a614a2410a3bd02c44a13d309d0820a8ba84f2443ad376f0dc3022ceec6f796cd0ca20040ea203333fd2684bc5788110f28d3d8678f1b3ce9c16f7c447d837a39acb5463ef344907a7eec801c8bbc9c82dd2bc85519dc73d88b77a5d6144115976aa42ee99e8d10b297edaa4a147708775143800f7eab650a929da527887acf1f1e197ca857f5c2644b536e5d027eab8a8ecfcf6c58ef9037ef1f22eeff80f1b78ff436864cf218f254a7c09e50834d6c9a70177d73b7d13188a40f5e5ca18dfe10c425f3137ff78d47a41d13cfd032de0c5987d775384cd94856d06e54ece70a253e60ec4cbe101c24fb99aef996cda771f9608d6cf7ba763cbed2c9d4ea43870fbf87d4d035bc695f87ba769bb3b9816d74c50c1c7034a6c6f2d49e5b6578da7ae73c1f4b7ddc3b75073765fb302e597447aefc3b96c170a77c7a704566d14155b87b9fe909667eecfc560842e01673a320e03fba1bf2c12182396049339c6c58f7733f4d0e66f2a61005f3865584e2ef743469c5c0afb87223c354de502cb7f76136b0f4990c79a9641e1c3661e6c8729c4d346241945a4492149505b5663b0b143796a82c5106f577b9754786d69e38bc05079963f7bf3452023b3fbf33237cdc0b4d096dc5eb290c5fe09d73693db8c5b3256048f00f7ada56f574c34c57336c5a479fbcc09a07c6c4fc8abd2f12dde924b783daa916f032abe7fafc65d4a5e46d9f6f314d6ddd38fe1c89733b655fdfd25d0fd5c8b04f5bd3b600e437a011680474feaab23e9bedca6ad2df0b9a686efe2ec535ff62950b40bf78ad65f1d0e06507fe809708ace316c0b4bb737a63e920391cd2e05a62b87c4d6de1e57cab51184969422bb608cb6eda9d330425f47d90ffee7435e2cbc498d302870b3be895c4b6e8f47461f6f85f7ab9ec27a573f3a972d4d3fc5562aad019f9f36ec850b829440c52e568267c8665f300c9cb3734cc56198324d291a4f1685e24f79ce74a1bd4084d9afd43577b93b1f6741379ee26b4fcdaf24810ccd384c2c69f7f19b641349fdbe454c737e9f843c4ec8ce91cd890271b48be82b46973a60f36b0da7cace8040ddeb25c1cd69d2b9a52a8c932ca1fbb4a7829d58a86fd0bc2d3c25540d7fc4d782ab0a18e4d09f8e731cc5bac9107fba407d1417c6e6e135186a6e632f64bb9ea49dad4b9dd6b0d4ae28deab9950716bce06779b8ecaf9454bf8d3a5ebdaefeba67932fdf2cd3a1a0bd456411d950384d68e536eb45dd377f6f70f8db620ba9f1c7390dfd0b7dfc290b8412425c0ecbf2f0d0c95355d7b03483aa59bc468a0752641035c15d7c6251e6af3b5f371cb70ce2389007af2eb67559f4c88324869b3f70d3ed3c69bce328b0959b8934f0d0bb3e4d381691538ccf914d54bc3e1f6ff597c012ed20ca7444a891fb6461fb18e82a39c8fa8b11cecb9f752a8e73f8bd4e9b75143602789b44a6d5e8589fee010ebc3fb182fabf0da45163c7db9ee900ef30227a327a6ad7e7af1eb1629fd209006f3ebae68f12474ef065fe625c011facf362d66e0b3d03ccd21c5a875274f800b9957274da04a36ffc615eb75ad11db336ce53c595dca14431ae474c7510d29082e57ed2d7eebdddb444d01280d9be17e0615f7dec70d814d498d62461be9bf06435ccc26fd8c2b781a88631ad0b4665fb95550440efa143b5973d5c7851ac90783638bdc23ec69defadead59477d275268b701192718a09e590616077cfe5f07a4633082d1b7a072b69fc3fbade537e7a11aa27bb3e7d50cca743cf35a75ac08713d3bfed57b733e72db4e87c29aa565818075294757079c1e18c98656ae5e0fd8343cda0e40567a1779c9c95741fc6e29af9cf61ad1910ce2f58fe5ed651504a4f7da85afa6561a658306f65c997dd3e10747802f31be3e842da64a21a8b40c59c452ea14df1377e83a166c4359bb5201eb198d019cfd8e46a960bbe724840dfdb4a6e1676f041b487782ab80dafa0f2e1f73e0b5ef9ac10b61d91b0f4d6e0a54c1c49acaa9a4460d40e0946bed3e067a18940adbb53902ce68dd59e2df9200d63cf5b5c18db626d3bbb69bbc55a71997791c0766eb80bc942061af2701ba0459799860c5e65cca19fe8b593b710d6d2ae90571a129d772bc8ffd5949d85500"};


// for different account we can have same tx in the Transaction table
string addres_of_different_acc {"57hGLsqr6eLjUDoqWwP3Ko9nCJ4GFN5AyezdxNXwpa1PMt6M4AbsBgcHH21hVe2MJrLGSM9C7UTqcEmyBepdhvFE4eyW3Kd"};


// seed: fall lava tudor nucleus hemlock afar tuition poaching waffle palace roped nifty wipeout fierce mystery thumbs hubcaps toffee maps etiquette jolted symptoms winter abyss fall
string addr_55Zb {"55ZbQdMnZHPFS8pmrhHN5jMpgJwnnTXpTDmmM5wkrBBx4xD6aEnpZq7dPkeDeWs67TV9HunDQtT3qF2UGYWzGGxq3zYWCBE"};


TEST_F(MYSQL_TEST, TryInsertingExistingTxToSameAccount)
{
    // we should not be able to insert same tx twice for the same account
    TX_AND_ACC_FROM_HEX(tx_fc4_hex, owner_addr_5Ajfk);

    xmreg::XmrTransaction tx_data;

    tx_data.hash             = tx_hash_str;
    tx_data.account_id       = acc.id.data;
    // rest of fields is not important

    uint64_t tx_mysql_id = xmr_accounts->insert(tx_data);

    // if zero than the insert failed
    EXPECT_EQ(tx_mysql_id, 0);
}

TEST_F(MYSQL_TEST, TryInsertingExistingTxToDifferentAccount)
{
    // we should  be able to insert same tx to different account
    // even though it exisits already for other account

    TX_AND_ACC_FROM_HEX(tx_fc4_hex, addres_of_different_acc);

    xmreg::XmrTransaction tx_data;

    tx_data.id               = mysqlpp::null;
    tx_data.hash             = tx_hash_str;
    tx_data.account_id       = acc.id.data;
    // rest of fields is not important

    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(tx_data);

    uint64_t tx_mysql_id = xmr_accounts->insert(tx_data);

    EXPECT_EQ(tx_mysql_id, expected_primary_id);
}


TEST_F(MYSQL_TEST, IfTxExistsForInOnwnerAccount)
{
    TX_AND_ACC_FROM_HEX(tx_fc4_hex, owner_addr_5Ajfk);

    xmreg::XmrTransaction tx_data;

    EXPECT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));
}


TEST_F(MYSQL_TEST, IfTxExistsForInNoneOnwnerAccount)
{
    TX_AND_ACC_FROM_HEX(tx_fc4_hex, addres_of_different_acc);

    xmreg::XmrTransaction tx_data;

    EXPECT_FALSE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));
}


TEST_F(MYSQL_TEST, DeleteExistingTx)
{
    // tx hash fc4b8d5956b30dc4a353b171b4d974697dfc32730778f138a8e7f16c11907691
    TX_AND_ACC_FROM_HEX(tx_fc4_hex, owner_addr_5Ajfk);

    xmreg::XmrTransaction tx_data;

    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    uint64_t no_of_deleted_rows = xmr_accounts->delete_tx(tx_data.id.data);

    EXPECT_EQ(no_of_deleted_rows, 1);

    // try ding same but when disconnected
    xmr_accounts->disconnect();
    EXPECT_EQ(xmr_accounts->delete_tx(tx_data.id.data), 0);
}

string tx_1640_hex {"020002020007d08602b5019318990e8e469607ac6645d3cf7b4f5db9d614602e9e956c63c35cf3da6f7b93740c514e06c898fb0da2020007bb76e15af107934aa55f8156b6163ef54482411c54a280081a87fd35e542b3b8906b702123c55468e71dd620d3a402000254b5891ba8c1de2419bcc877383f004929e01a8ef8f9b5ae87549c5a5284d7710002a9d876b01eb972db944b78899b4c90c2b66a3c81fe04bf54ff61565f3db534194201cbfd2dec16b9cb35c8b5177e18615e7273b2bd43517dca8bd9ec9aaae6b7f67d01cbfd2dec16b9cb35c8b5177e18615e7273b2bd43517dca8bd9ec9aaae6b7f67d0280fa9b98fd01d82bde315b0324aa236d87c7dd34bee321eef33186febc0a38d0d57090027dfa03aa67466407567b7f8c1fa4587c003fe89426787b1a96f30e4a2a010fc685a095aa65eb5c3fac215aaab75565381a0721d50199fb902f58bf0dbc7a19f9ef086da5fd12a08dd89ab63e8becfffc1b2ad6510e4753b06191092e0ff159d18202a80d1e7a2ac0ff89f7151e1d8736b4a4b720464a262f52bba0c2ffa5355b3a0bbd5a5a87a2610c12d2edc410d2d26fc855c1ea3ce211e19881a4bc79b7ef28045951e126e8a05f6d69a9736691988eab8d711501f8b9098d9e4338c1da65da4fdadad568ed09dd2ce4f22b8375bc3c8fb7391af13ad24d34eed99cc61977a853d1f98016ed8610632bbcc6c31257359858afe724492953d2580973518efd1003e906f2c04ba82f9a758e2c79b9bf9741af8ad259357e31935fcd33c02167bf0bbebecf0529db992fd3634a165407ed6cabdfe53c794f0070ffc0a2f862ea720908986e1b8c458e8d3782b22ecfc230df2e10686b3c4ac2f801e308a248485e0d2d005cc47dfc92da9d5b57202e1a267784d98bcb2bc385fd8d6b119ee2ce7e0c14a7991413bc310871d53fcbe2d863c074d136ef4b912e1a93d00c92f777cd024bf4b84ba5688b84ef989a3cf56db681bf973dff7149610fc9c4f901f0b2bb0589cf064b9da57fc5506dbc058a76d7a960bd489c22dd58290b552359d53e6e0bbcc2443684d6410ce8f0c480f719532022a52cd0dbe87d8feb81a2180e45e90b30fa0ca3b81d6ad3940d78a181691c427c4211042b556a054388163b49ee4b0a188d4fa530af830ad5bacfc6333e33943af4b4b8d7c34293d52bb0e5a6259a06b2a31ee6529d55860ccb9b6b087bcde693afc82cb76eda4b981f8593e78bf703d2cdbcf3e2ac02b0ec66ce864ed5f00ca70acacaf87530b3d87289b45db3b007a080e9cff29bd7fd27d5ad5e978ab0eac1a4b789a942b8f37639cd8df0787a023e1d32610f6d775262974b398bfca302c6c43bbcf9b27b2796fd1cf7d135c801f55ca6595183f79d8ec32a1bcb5a61314952b73ed488fa77622cb3d3cb7b43049af38ce804cdfd5055c4368ffd8ddbf83ac71da0949079bae68ecd699216ea086b4e32de20a89284cee23058ecb2537719fd1c5d9979d88242a1473e58b91e016905d79511ab3ea90aca5722982099cac5e143ccea4d9cd0aba68d3196170c007a28fd08646dbb02c60e37a85e978e60dc5367322572c9af86f60a4e7893000d5c6acb5a1463bc3b114f14a00679b3bcda4d9f4ecbe12879c83f51debad5d6044905edef20946562bfc928e0dbc504b266dbeda3f8d7a4e1adf26019176626010a0e53a56cc387bae895f22d8dde4e4dc07a8cac57dfcb974c225e94462ae100b5fb06aa5f8829d9d8c318f0e60a609c9a5b0b5646471b939044458df98f33095e034929c8095cf28ea4fa2f515161e007b7f63ce164f7150ebe7df571ee1a0abe14e63260d891f8d800019fb32b6b421b683b9dcfe739bf3e85c1125a59890f6259f127ef8aa309f226506cbdc7e0362d327c01b24d4897c42fbc9c7b7fd101287dd6c1576e4db50eeaf9512902d880e05bace368d83cd0a3ed44fa9bfb0706a7429addc2491fb0325bb19c8996637c307851374e7d27dc09e779349c2c06043b5efd3a3cd045e5fad7e1735dceaadbac2e08189e2b1e2138fdf1ff783fb3093d0486430d62adc0687fbf8547765ce63ef4d637fda88836882bc6fbcf723000c84457bac63973793bce419d5a2f3b2b6b72c84e8dfdd8799a3c2d37f81b0607f8a09809afd99e51262b5e372278553504bb5862f7b2a57d05296c1bca79b407d1c88c875e7f7212c38b64826a83ac2fcce37888b86a870eeccfb35f4042470e3d0ef3a4fcf02c645644e33641de834ab3d37315dc3f508580f6734e1df9e10fd368a0cd8a4e58c5d259dce025ebb299478712e9ffde3ecbc67e71d3c9eaa20f898829cdc7bb2ffc041df360d14bc683ebbc62c6995fd2ae5b99b433dc18ca05dc05710159dfaf2b85f65c1949e9ba64efd068dc1626d7ac3899d0770d083f01818867a3d18b531cd7d46630e9f9582e30e6d8977cfe02ee48032b84f2bfc908cb21db8ddf71d64bff7a67f605801d5903fdf6c7bd76ed1a7842c2be672d6a0bc3f4b7aecab3203cfafff53b4647650db10c3af55521491c31b57e9581a7410d432303747fc20fd0d34b978450df1448a50f5a83a5b1598fdc0964e9501dff0cd345f2adedf34a0772970e5ea30c6c30671b49721dad3ef863beb92d89513b0b3c5190b4293c1f7a90f93f53cee4c4ed02b55d6c7c7450f04a81cc4f663ab80828a65d7342c17d17dd366c6db0b05d58a9be78ff2428ae878ba4ca7091b1910b120408bc946c2c0f6195e5a884b3399cec920d2117ebe56d759b51d5b4d838048a4322bfa22daaa42a5b7d9c2ec7682926e5e8473335905a440b50b4110f6a051a8709754281fec21f7a9252e989dc127c489fcef630d7d12c99b34e0fa101012b663304f27a9ba7b1391100bf2079c98b3dca37f7d6eb6f80d5e032635d8303ab8812cb9e6d8ed68f4bb505ec2ab20b2c9bd14576b4ca38a5e264e104ba730cafef08919f21b09414b25a78c680c496e0916925fb70cc26bdb92b7b5b7a5c0e539c8a67c1ddde3243ad30f7c0f0873686b7a6913df1e8381c0067183e1c2a0d529adb661cc18fd226298bb6040411dc169aad5d6f5e80bbff00bf5635d9500954341fef317b3ef58bf428eaecf52567ba995fd126ba5dc9328824348f827208f4ffa19a4bf6a50235d9dd6f59d6231471ac31bb4e2a70ff91652ae11adf320f73d286b43d26986bdfa07f0740c8f5091ee1f2aa01f76a04ccc117f50eac550f2e63d76734274e520ccb794f614902edb433449d8f6376ebb3b0ca37e3dc6c00e264e9767efccce7984006bffb606559c4933db4ada7e48b6cda74fb45481c09d5fbcad64ede0c35f31e30258a7b13201723eece318c6dbe9d4ff0d930a3520e41b7c6242177abf7e55a30ad955af77fd01d8113c93fa218164256b51ca5f702a1729aa6ca79da812c5fbc95f66ad0b17ffe1e6738cd459fbf188495aa58ca0820730ef1462241797287dbab3305762995b2c26f127a3d4ab77f5997b3137a035d99ab26278f1c1f55e6216d43f89c4ed464acf6bcaa00abe2b1ab1710ce090894ec9c3c037b1d9ba3781218161a061578fe80e66023147a6aa28aabb6bdeb0cb7f4e362fa41fa067054018f785cbaf0fd7eb0b00b706e9d78bea4c8273c4b04ce5fa38084f13781913e645f221fb7c813ca90944d9e46f28538f2fc7a1c1a07f2ff0052c692451b1a0d099fece7a9b25bdc7d0b4a88d12bd6160727c54bfb0a2dc11baa3f5f4439950a4e23f88afc673e880670d5faa29e339e45dc8973880587daf25544f2af5efad48846b4750646b417b178b127e4fb7694b72a9827460e5c041fe13002875d1850f66bac6f153231122d51b7948be2a1a45e0a66b9a909e8a9e93fef114884164a388743e048e0a9d262452ec7c494ae2974c41219110e14df0d487e820924fbb5de949b811bc2f7ce2e31af6c46d67d9e3fd35eb0ae0aeb74f8add01ea5ad7fa708f129670a636c3224dcf1c908ea6852afedb152920b73ee3e0594b50d05f22059a4f536ac619ac8235e1ad675d6b5207fb29f1d0f013d719023ea1291e8070f7385af52cee9432e2a05236bc5df2f6e79c308d0950192f6cb1dfcff809c1b048cd9796dc27750e51ecaeddecce534e26510c1a8240db192b4a996982066dd1823f39f9cb0cf5d729c2d608d7a25b3f9620f9998b50ad13c7e44919cd6f8d49127d6b8edbecb325a4dc6d2874a16c4bca46da49e510cf51030d536ceba148271c9b25c93727c17ad6c8664313c6d82a8865381282f0b2df3a52d5ba494b93e0a38e4bd6ab461167d50b4c566df3647cbdcf63bf4c901903374972ca9bbab1c46788e554727267136b2c70f4cdf62a758abafefd48f06650c2ee3eb0d4adacef120c3e602d8684d09b7bc77dfe7abcf6d879051aaa20248069e025ca07df2f90a5cb992adff4a3500109101c20ffcb09f4050daebdf01f2d3b3b2eb988babacee8e3f3c2d5243fa932e9554dfdcaa245e7536e0d7470c5fd5353518544bb620da91c5ea12ee5fdddc083c91c0dc235033b60e2d525200ce293bc046f3b04cf0f5b73175b0ace5c9b295a15f9fb04306b73b21ba701109e348765264529a1bfeb3b0839ecaae94d985c99c095809d992a889c4c0ee230e83f16d11cd25d172336dd11c1b2913aeb9ac3b90d8e6db8654d623b85fa15902a61b8149adcb35d0ffcb72e804d36d41827c327139a0f7ecbc2685e62bdb870b05197679d6509b08fb458c2394cfab860a70edafd37d52af5f42ddaeda125d092eccdce9d6c6375988f2a0d93b72eabd044fa1aaa4ac0f71f33984114c4409038780d60a500829d4bd2302680965e3c3fe2779c8cfeb22abc37034df926b1e0baffdbbd2a844fedfe523c9e922f7342e53567dc2f0a1c52e3958d4dc21b075080c42427ed1f287bcba9e202daa5e1c216eb203aa52c7c6b00e38c865ade17307d51082a7ee880b64487fe16fbd1abf9e2044817136f7a571caed2d6eca24c402965cb741098e1ad46aaedb55f97f4a6e7bf96ac64262f7a16cd93497d81a6b0becff75632838c6fca810c8ff8f7bb8ff6092472eb0418c6feac0056314deb005887adba7e19af0fda0ed694c041709b6739fa6bc58dbe4d9761777cd7829a50926bc3814ec4227e9b81ae3b48186fa683aa721da09a06444ced71a0c5f8abc034740ab09c1b9247a45bcebd8ea1d57022d5f47f2fb41eb75fb7ad4c23de48e0d70fb25c0ff68db1ff3ade50889548c22782f8d7eafe4e54c36d45538158ee00c69e6f3f7fff561ccf532c2e1a1b870e52ab0c380fa7ef5fa9c93144a3469b20b7e2eb5f6e13f5a3eee6631cb7a8673f1ac175fe97b71c613336e2ccd7b6dcf058161e1f15eae11460d3df68bff20402f9e4d86a4a842d54bf8a150dc10b25a05f5cf57fb6a7036a55b880c134828a9e39d647ffbec94d9e12dba7ce6d7741702fff89137499e9ad8a9ef2e7c685101ba98af0c87e9f132c3459d8942c00442080f13dea6233c30e46ef46fc2f8c7deee1bd15568d08218fdf24d6e0d20730d04e3856b7556f2dd6ae74838bf189e4266bb069be3836d5d45cbfd3ed5856cef08f0765bf45ef900d51c0a38e8434ac9ab4efc1af79bf952f2dc3ee36b668cf808083620b61c6040a9b7635eb11ed847dd15834dd1ee826e4c37b51a8aabf6510a3a08e79d2bf4bce66dab207fad2e1208600e2565a3ceb34c91645469527f2502bdb433e1106b87dc3f405bed626932bcca26c1fd0da2c99fe40320e2cbe9f00515d6cf6eafbc6f86afe80ab532958cc34293304cd5f2e795eeea56728a77da01acd3d807a4a3f26d7e61f68d4ed5a8c0c6a725be564edf9ff05fab91e93df10c3ca8d3f9466b98892f7d58383184b2113f23339efb985a5bbf0e116230a348094450178a7345fd8954d0438cc3e0b8e0d53e6430d7dac95ade2f2afce72d1f0d694295c988491bf33586387fd59c3d807dcd71fd3821ed6a4c744610010a98053a914cfa2a27fb7d6144000e2dc249556eec289c4a9d799cca46741f34bc3e050a6c9bcee65cdc386a62cef193fa8e1ce88b5dd144479f9114899aa54a40df061740d21203f8a985f656448159e02f60b63814e1c39d89aada78c42290f72a08cc7587d0fadcec62d993d6a3f5fb698393bd7d207b5276ada2bef680c3df2006be338eea4921c7ad3b8ba5f8345ea20c25ea25da97dfb48c2635532dd502e709287f1a0be0314ce39fec53eeccb5f22b3d705ffd26a91020f7fafa3597120509fce00480ae400649bc8d80818b066cc397af45377840591b6cbff9a195285a0fd6c38001691e8b7b8beb1b70661b9ab337fd067a6ed0074ad5f02008fbe0cc00958c406a1a0176a46a3880fa7e02294e32faef7ed3ff3cda60f46e00ede3010b9bfc20b78066b7b13dac642a3928ff7cdfbefc211be16503f14021c72815c50d9346e3c75131230bb95791809b7786ea86691aa988e454e845595207d90f64036eedb30b30a9e2e1ffc4b34f3e5c8c023395cae9cd876536326c0f465746ed0ce36189eb31a65dfc2cd37443cbac286fac019d56b588421b86f3719605f4451560d68b1b6894bf0c4c0e0bdbc3ce62a74ba93512e778cad286c86e908ce8278ee8c9a96dae82a62161dbcd9a6559d2b66d9648deaac79409c826b8497835bea2e3f13a09ed68e9c29cf39d096c2825869bf9e7332c3424739c6293a5f48c43145ce17692f00763dcf111ff6fe1c33eef1f1b9174dea7b52a94e04cfce8828e239d321bb7c1dc23e25d97bde9799df98c5812762b7ea05cea4f8983986a10ede3d178ee5cf537ab87dea0916a078f737a7d707588321e44680fb58f73896ee4680317f2ab6becc859b4c90a3deb5f921ea01c9a2b5354b418c9059856df23b90eec5859b05d45b2199b80e875073873659a1d9f85ee539f5edf9c5cb0ddb5f16a0d6e5f6aa1dd19ad6b2b1dfffdbf73ac0d242be68c16db992c8493969ab8aac727fbd1a45720443aad31e25d68ec63d831305681c38c64481c420f3c4bfa53b0ed3f230d00ff6308996419063c2d1d77eda0f87793d634a92581b650faac14c065fbcd00b5ae38acfd55807f40e181d99ff45c3a8ab75a04dba8bd60c739ca100bc5c74392de47d7beda4d9acfd29eaa7bbd4d1a2e735715549f7e3ccc550e1b3925d78105908d61b63b45fd9f36587c2c462f54d4c5291a0775454d94a7b6d386af4ef6c986ea3fdb9610954d48c6da970c797d93cfe9198eed598e27995f421dacc696a663d543e53d3ba3e1822b9ae9f2d38c3b00e73a7ea2c9b9faa586ed25ce07804520e12d63e49cdf5ffe9eb9d445195aa6e74d0562dafe5f0f2f947579a3a91448de9bb75484cca3f4dd437aa0800180252d7550332f7e94d9c6b04b7ded8aa754a400484ec4a38185fa2a92eb0c2fe8d38a74a754d3dfd51f246c3476a97f1dd805185c176f61028733929b5197b959acfb1197d3ea024cf6d3d9df5acf9490afccefb397b2b24d55347adb3f97ed5f0a3f411a4d57ecaf1a7000e48a04bd40314d84e0f7f86a9d9eda2a3f5c21a84432bcb441274dbaa9630e2a544479be2a9a0aa8b26ec1a5258d4d7042c2c9196d4380d6878dc62f8714e4a93d4eeecd970b3ff39b351354b431a1aef5ee9ad685083c7b5c5a4e53722676228f5df06be6df850dbca958b4e4c3fe82c644364846d70e0cc6bd28ff12ee11294e4fb05afef41f80094fa3458773aaf3409db5ab4315f7f279a95a23d94e5da9a5e2556fc25c92b5f15f4abda4cb80a1937e3fb954c53e527e24bf480079e84333f7b23fd0f7f70c911c2b97bebdb596c1fcd33af9155c282fcbbe0ec971733f3549a2fe9dcf8b3e0feab2f3dc57dfa9413ffede85bac8076875d0461e8c4dfac107b9aa696426863d7b3bba80fd6ed7e349175c506587a773c3a4937e8b395975eb95175351ebce82184c8ffbe8f08435b86d72b2cb11c851ad508712f09c2da6796018195a45186004e274dc023b2aeb2c0672b873176005fa72bc948b5de2fbd31954089a919e5b2925d632ae7d54ac4466f8f50d66ead8a98b9761a0531d6e79edb6cb37c9a3eda44a4dcc9ed89dbfe3f82ea184c1e1594c98b18eedc996f5c33fa72619f782f39803210f0510db63251a7163d4a0b60059a95704aa5e2c8a2e26e89ab4a07ec65cd5aa840a0c8439c94b1e649c7436f84765402ff3b8f162c541beb9d821414595a40c9a9badbd3cb4f244db8d69f0bf8d544ab3ca46e5a225110f3ed57ec3518339f8d36bd4cbdd9a2b28b4a0a36f9c9f6ff1038d3eeafd323705d5558179a35db9c99d99ceca47a266cbb0b6563a8b85b17b2a274201953317b9ac2bbe1e50c403adebf3abea6958f153743ec3ee353955ebf89469b2fc2bc2464068cc3943bdefd23224b0fabfbad9c681dc921f38c2d75ec22f0c02c4e44622c39a43a05d46031242f5141315843f5f8e67539ef04a92168ad6ea5cb1921dd07382e9446304748e2b443c90d139352a725866f1a7033c566a86e4c1d15d02066473abf49919bf3ff17aa58eb456a5ab68f46c46032cdc428e5a2189507b054d0d1a4b633668b5250568f6c6f6fe6c23a80317b032ba09166a7ad025dd6571e2cf04ddbc5f9fd27129715ed4fce30f7878721dd5f3b519c957de9dc14996658d83c0929f5f402562c84875e8e4b43f91320b7607835bc76df4a7c2b51047a36073e1cf735e0f973704bdc7ba7934430716dc0ef255a5ee64e4dde7cef7f2496270a6404966f91ca535ad8a6d2e2f97aab4d8370d648734ba2e4924795e63b24a9c38c9e8ce41ed08e52146f8604b3d03fa96ee3e0b1841ca0bc80bb6d1cbb4bc522daafc54ad5dfbfd4b4981fa227ce80df1ecc30bbd623e907177d3125b4ff2641191de37f8620facbd03d3d13c3be7b48b32162992c90b552c3af21272cd8275e32184fe41eac9a5f96ecdf89b30aa2b6bd85b251c63e1664990ff336a848612e97188c4d28e92ee44a0f450bf04cf3c5f520f47fba5cd5d4087f43978a41bd9b928f769c2fe65ccd0f2db99e039ae83fcfccc585ebbbe82b2c5a48bb36331108b12c53fd17ac0083dd14d9f2723af95ff4298e83b7b832c8c7f50c4be73b5f7a90e7b3b97619c037add74a808fa4f29f0057d9fd279bf6125505ff0f39e7e6873823d256906abe8f6f691aac9a3b61a6e8510a7ee51b3001538f41c1426501f3600a8613e60bf8d6b07f8bbb3d54b04a9c6f9c00a073b19e4b7a07e07661875927781d48c01ba73351934e07683f732989db667f717180a114618dd249a5bc6d8b89876d4fa9cdeb7d8d8b2ee21874d0e39461a81e6ad556ba17e4db7d72c14b7dc31223778cefff9fcc6725ef0bce9e9a330b35e7f2821b1b144eeb94a3d191f3d60d6ec2623b495abbb519bf3244a4eea138dd2670500dde9c1a017ef97db10f2f4f127a764f4332ce5691f85c6209eb85f845a3100d84318fc0b0e6003977f07887f8585b8240d842bdf056a26ce4a4707e73fa42d1adf18170256a4886222d7b4e1ebb3b4f4df24cefac4645038cc3d92e4ad773ad647bab005aed06d75e3e7dd2012886151e38d818d1056d8aaa14cfcd72810882ff7e8fc088de4fe84d9dd0d77317984fcdf191d93a5066f93179768d2b12e3ab690525c0a73d01270d90bee6f6f4f7a9bbc53a6aa165dfb77171e7fefb3a3b2081d5d0d01f04e0f26b784961c92e83493a5332ec26ae72d2ab8ca284a12967c13358f440cf6b0acde4d5db567a70f9233396bd448366a61a00211c344d91e70b87eb8da06cb37dae17329252ce4a070eaa002ef35df8a51fc735c59fae774351db839ff0d46bb3139c24f4c4029890ce35c58f7de84983bbb371750aa5e6abf3ce7ca28098ca8230372a1509f4e1978d42b27f7f53ae40498f8720195b45f183a00d4f40c029c96b1c979ec0ff0fe63b07bfcff4ce283ca07fb70075ab9b52db2aaeb79065f697f82aa3c484594ff8270c621084946dccff9b60de482767aa2a88d026b0212d0d00fcdeff9972ead613e958a701e407f589e8e088af12c6550ddcdabb200f61255ded6a6b1ca38d5617e6c905668e32b6338daf41020a3493d9ec1b5db0a7da20a380112671d3d751e433e49bcf6b81508434e556633242a907a02d1490b1a4c794d65f109b0c32aea5f313411b7bc5564a3e5d1c794bb7546662211740d70b9f5c4095391cb934f165a702e82fd58c68ccdd6725988d4df281f6ab34f0b15e2794312c3853c5342aceaac0dc83ca83c1134cc1ea524d5520b670d554700988bf01ce91d4a648bd8e49b3d36f4940d27b549cfce79f186abeda0bbfffc0d6dc166a803f2880f03e27f4b0b0c49ca415a04f48f18c1c6aaf18914a391ff08591d8db7621038174ee2edfe9eda59c1b51677c17ceab23a714c40cb521d1f0a01d1cc524ad350b16ecf4b3e99ceb3dc1c78a82c002772e3f463f7a8bc3672049f0d448deb14847b89f62d647d8efcf30f183aa737fdf852b74936d987dbd604dff05e351422d1062eb7cb89ef0783c87d61c328d1c165bb4bd920be1149b3097958c68bdce0b0cc2ce8a66cc114ec76a79647cd1a37891c66060fbc1c2e5c06fa9294fe698ad2b7a922656b15371bd8c25830772a498ed382c5f36c96f59a07ec38911ccafa43c1c15054f7d49d8831d8593f1258b6e490e1cbd6617e3fb605de6d25e17af040f4439d51aa22c45fe624c49b0d56ee5e1577f3fa6920754d07d21c919a216a1b0af774e4ed1f469696c20171ed652df83ad4572c830042f708179836e48aebf19114e9509b9ef801ec73efa5e21278e57f74c92b8069450c06fe6137fd9d33391682a1837b55a536d128b93d46a8fc137f4b760435a5ea000bec515ff86c776724e9d03f67ddbd0d6756c267dadee045516fe787dc42ca8a055fe0908647c41eaf9e3d99f39dc61440eaebf12639e534e2b8cd2f0c0ecb02051a582b40b1ed4fcad467aad4c7260572c42f9d4a002c6a5313de5e46c5362702cf9306c862d4880eeabd74dfd516fc307d78a81c0a7309a5086260b47790e10892c5bd471270a6232e8826dd73c0fa269670e5eabcd0b21177c77887083b75025ee2c0af1d3e234a657ae58bd508ab41b2deffe6ddfc345bc707b8b7eb842202bdaeb5ab2323d3a9f150477e684570b3c9b648c9d86e8af65346177cb8ed1b042f37f43f2707f5ffbf90d8b6c975305d94fedc7f2fd58fcdb668a14f232b9f0eeeac098fb6b0e323da03d95c88d7cb180a40f320decbbb3f7371e60971b4c808df2a89a7a3f0cdf3c134ea1ac6a7da978c967aebf194e0340e59bf3588af930c717495f9787326385182e27f6445ebb966de97a3b28f2ecc89a941ab8272000865b47ecaed9192a6b3ffdddefbd5370325966a64d2b83b0c1c6750ee7501d005223fb398b082ba6a36629d02dacd473e682623b94a0574ae66761c74737ffe0c72f66a9f23f7297ce7a37a3db893bd57242e2524652450646d61d06e629b1a0fbe71d721b18a643f3236e8fb13d1e017c4e88d3913cb0537c9efddde48b1af030451f07cef3ff65b6b59bae6f60c03a6cba17dc802e6a074421d30531339e50a6a7e7b33b154bf8157e6831afe50e36738a1077a3290890e8d16f8703953980cab87093bdb9e91220db0f1c4347ec557d46fdb0578165604931bb804bf7efd0fffb444d3ca9ff102e226cec2192cf7d90400321ed51ba66143a769002960b90d893f472d8827ebe4534011e87918eba2a7cb19b76e74b19c26cbb5feea73f30eb48389ed6aecd32b963532bcd80ab154ec6d4f96072ed4866a12f5743bae240990b6f75347502bd26befe323725fbe5ef739311f793569770dc18f71c0a3aa0f1517ce82956ca83537c3a2f5c596ffc7fb0bba55195c2c21bdd22d4c83ffe90d7dbc99a92b124572a58c9e4a7d0171caff06c11d54977265e2ec98e03a8d0709ceca0b9a9e5ed48557d11290279b5de62c2d42fa5edb9448899eb20042abcc0d57584cf791a07f30790fbef1acc15f1a2d010f4a9e69590b8bfd5f6da126a80cfde8153e7d45d68193483842896964c953a489b758df5a971a37687ccd9989059cbe19454514572a3a9afaebe6a70cc46aa574ec9c055f50e623886691ec5a00c48a0ed80702f09220cc65ba8ba0eadd55a18bfb644d3a9f0cf1534b7e8b5003651cd05a5dbc68dc97917e7318bde04523004b55c84909eb218bb35ddf899f00109e0eeaa0ea160e523a32bc3875a6d0b3faeab9da80f3cdfa289693d3190a04142911169bfd7eca5c300d0e5a69ae612256e410e555a0ad0045ad406dc04808dedcd2b7288368eddb92834049d3c44b8c32b642ff64bcc9e0b5f4b4d1188a0aa81b3c55f3b6ea6c0bc975298c99a443b8acffc9418afd3a1298f1ec0feee90b6481a44cd4977ae5581b9ec4f4a84984f5a54b67d1a41883b3cf30727365d60a943b81637974b4505a8ce3594e43b389340cf9a814d03b47faaf9d674ad18107bb063d1038b488b2a0a906d9194495c9c1be5e8a1feba17e8fe704414b58c70498412044e664695a47b4f5ac8996d50bdc3e86d3b73026b39d665cf4daa0860fd68a6ffa67159bf147cd4524740e9e267e4d4729a76fa4682b305d80979b1606b464e0e572c4f64ce4650331f5919dc5328cbddce65afd40b14f0cd57af3bb0867e756870841806cf90bb244ab6ce9cb5758dd1d89178ce5f7d3136b25745802aaa6a6578792ba1d08d756cdeaea021a6a45c1f1e1212a59ee4989eea80e72004b55e2ab120c3ff8458460d70475e633267d796c47170ef500ae46cc7b8d4702bfcb3a84e07b89672fcca35bcf4b77368bf0a67608b07606527d29a56c979d0797e66659cd569507aca4fd8aaf2ba8a96df24666c627c1f76d31cde794fd7b0cc8f9d016ba903e664ca20a63c51c9179968b01af073f85221f3d3a9b16eba30eaf7e6fb86cfe5396cf7f367477d14b15cbd586ff612af520e80e27fc5c182a09824811ca79a42e124063a5c28a918b6ab1ac228afee4cffefeb8deeb0aa6340a987b5fef5659156f24f18a5a6d5a167352c6ecb51b2f5dc1a8e69933e913f5087b8a2f861f8a9fec3d8abece0526b9c4ad0908182968bab6114b64c3559f2005c6305173e720bf9d4d7022de294387e77e5597e815e2dd99fde84ddfcdff7209f6d5115c2a0a0470975477749522f7df02e4e33dfe52522cc1d28159da46a30ced6cc8c78e840dce4d705bf56984d0a9892ae23d1b47d959b58b8e43d64ae10ca9a6a022ff2e020037dc46a290b480ebf0ab84ea5c35f6a484b4f001b542280585d1e12e78548ded37a4ce76653c48095b78dcf8e7d1e61271e7945cdc49dc0a165c8b172b43891b0dda51a31c359e94b72cccc17b4261efcde7dd1f100ffa0308f75d9a7f1bfdd78f82a8212c91df247c5483d83480625a0e7a190e4fe8cb0fe5d6c6038d7c4700ebffa02b50ec8be0acb36c3dbb0fb9ad0e4a2bb89be1ae009bdf06898b901a08b8ed0ec7d4cdd6b56b8a633b96cc3e72872ff11427007205bef412f6dc6e72e0ece54d254b796fd1d3841eca121342925fd878b55baca40214b512cf5e6e762730d3845b7d62c12809aa2c774dc11b22086540d9f89605064ab3d2dec1a48b1c66b594fde8fc3936ecdc60c819671ee035c4800da3400809703468dce83e440edcd117b9f171e9480f0fc3c5fb0558f8d03db403dba8e90ebfb469d0344efc90a7cae60b2f7bc973a4509c36b8c8b2605fa6495aea5c420d12c5da36e28e86a5b1e19cb8791b2e34143c48f9427a0f6889bf67535836c70282597aa5cf1a1c9830f9e8662237c94dc87a80ddd457025e7581dc4d7b2f500798a211bc1125fcd5568864f920733c46fc314d786743eacb1d8d6c3d3c75890ac10a3764a62c8355d06bfe42d8c2be524212ec2efa5eeae13bd7281ad6bb8a0f0750bfaaa0e6662a09ee58f2e1fe7ad7efe4a77856acec9e3bec5885ac84900b2f0b087838c0ad5dfbb789981f1847b3e101fb8b3010a8e05562cc73e619ba04ab72cf831da54fe7666f4e6e23b54b58390da61116c05174b5562cebc1e61801585519d90768fd63dfaafe821905c03886ac3c4ab8653265744d3fc995cf580dcb5d87fe7e176b33be82d0e9359453eda2d0a64fac6adb7e4c84caf6f5a236049dcc46b0ddc33504af75fedf553bf5626c5a3e182ecdca9d73121e370ed4080b5c873cc2a8ad0aff884c5209c8657d6599d87a291b60b4dff71565ca835bf4044876d04c11c21592c1b76d32ef93bea8d8fa960efc10da09202f9a31cba31c0414fdfe2a166b317e74c7ce66429ba3598d2a279e460d410a82a091b527371f0078cbcc48e33e3e737e4eec08e789b670ca8a301ee5451f62587a0b221632b00eda21598a9d231e3b9cdf8901095257f91ff193b0aba0e80b5ce6f01f6dd87f0d8ca74c2b836cdd39beb2b2e9c56a7ed34557ab0c0e242a7410dd2bbe201e4900095dc8cd1dc2e46b6ab4dd3f88be5a362edded24fa7dc84e6f22a0065ae1870e2db17f06cd53c29939b3fff04bbb062b21456ee10d3b4cc68502245f086f900b2a506030285f591130044f5cb8b4348cbfb13cd7d2c237664b0a9ceb042803041c85d43757375195d903ab89b46fc8837629b0d113c6e191972150e3cefccc022a274f67276a362ed8475165298210a60bcca8bfaeae64b23a2c314f9efcb60a78a115ef60a1c2e3879cdd3e20e0401a67ece917a438f3b64953046e5768b7086c58911b01ee8772c2b5082e78cfcbf32eef2066280c32b647c9c01d90ab520b4215a9edadc9761dc0bd7f89e848b85fb2c1e0ba890794985a4c6abcbabc0c0f0612d486e040f084c45bd52430c7f9619f0433caa6ffe673c92387b04f8b3409dc55c2367adfab904b7572f4c9a87d5bf8590fb06aecb6beb952e06c959d3202878b36745448bfb77b99e04d08091a3accfe5c5718d75d084d06fc542b871f0113a4078a649eaac15893eddbb6495d95659ffdc26e5d7f1f22acd574ec8ceb0882829ee7edba0c9fc661eb18391959ada211de34c2ef0809345d3ffb9ead7b0d4460d57b4dbdd6076a6751198995f45d4fd5f7b32440e3742a00b5e2ce849905df13f3d951b9ec11d20cb29ad26c0bc8b07b3c522f09d027e337d6439c8a3b0d865a04531228cac81b93b3f3b0795108010312d719d3313ec2b71b26419ec20c04a9ba6d993f356bffd93bfa26fcc52e5eadacd27af8e652560739976c5987082a37fda65e9e0ebd5a52a47b845a57d2e1b3e0523ba6b5f06a51e328c88dfd561c4b45c4c5e2887a7cc5a0b71591f549258c0b23351bdcf5036c0db867cc7eb4a268c9e08aab7700509cd5097d9ef25a1d6772cbb915f337d45ad64e84301cd13162c130504371e3bbe62e02194c2cadcabda444c42ca9a1fd95ba4d16eb6757b19072d92b3c2f2e8b2c5a195c5f95236bded0c7e0ba6ded8c4f50abeb10aaf162855386d41f43daf1377bd478bf04c6e89670335d9abb4b67b1a402921ce434eafe3ced979b46705fec26fd073909f4a90115ffbee2e3e645f3653ef54a6119ebb31c96e47f26bee912ae84322e85eeaa54a4feb1d3a804f2e637e982b7f0367754d27423905e91ee3151c1284b0ee2c7a6bbbbf9898bc24da61a8ac8673855709f0fea1cd32bceb681ed41995b6accdc39669771c86d2890fd936805eca6ff1438a04ad5220dcfbdc99260eb9c3717f641175f8b5d38ea68cebeef36691b487277495cf1ff2643b173d8fab6234b26a54284a907f0634ee1d26cea69a6edeacef99404607b2d52500c43120786e74884cbe2ac4908e0937ec45c6e3066a4a03dc02035e1bd3e689200a54f6e37e48cf0c929c178da8acd6da2722bcdbd4133a032c30b33cd07d08a5772142d137e524d7600ea7db2ad71327fe8b04aa57c2b64f1e09279c373e354eb825163073c8bc8ba39206ea251be37c2e846520c62ffd24990675f9bca10f78740d69127b6af53b6744b115bb3e6866774a43d2b7b16962fbfd6df9be1ea25448f8005e53f6cf70429982ee7b992f1280664ad8665792c51aec555f5d3f3c61b5df4c37cf0f470498b072bc7246b1e4571ce80999a7a6c96ab1bd96ede46f84ac3ca4f4afdadd578fe642fa11378ef346aa21d30a7ea525e5b78e87a6dbc8fc67ee2153423747403fbf57c5228aafba48bc82acb6c81988f9c3e2c9049a692b1e029179f725f0005bb2df2d120a34c1ed007668a5ba635bb0c4220449d2fd40ebca28677963b668f35d0ed0035b242f206cf9079ab3bc41a93eb46bf567c82ceac29959f15f6b004a518acc3d8a414875ee5ee8266602b92cbe9e0d2d17fa1a190669d3e26a194f359db86ab469da95978318aea5432b0d68f2c1489f178453b2f42a64fdaaf123e3960b3b0911a4f70e62e88188f5dd1f873c99720271340432bc478f72567d8508fb53309dcd2641f010d3ac1c42016fd43d29b5c4d67b51610605ce99a4a9043b71d13968218f06db044f7248541735c09db726df59945fa57ea349b711577456d51581850c035b8e48f6788423834a979da19ce68a3450332d2e9252f6aefa00f99820ca56fd319b47f1113461658f4b0a31194f38c04fc20d5f9c217a40d1eaf539617e4a087476aa82607b44f5c7208cbe1c2ed9d6656c0e8d7f7b43d81c4e83278e322f4905f7d4d44367faaef40103e07bdb897514d9214df89dedace258763df7194b99d12f9e67cb16e594c5e96742dbbcd50f56fcbd560e7fa4c2b3d513b31244686f987db87dc713d7fb13b8cac30c6fc8e0f190b9a3c150a56c0fde7c674b8d020dde2b47ba799076bf1cc8bb756e7e8c2edc691dd4cb5bcdcd5e6cda7b144bbcabdd0a5456bdfd6be648c7165872c2a8ed08f7b399b6d78d7f5e6bba9ed6eed7d3ffe9f8f98e31b70a010ac56aa1523f61382f5e9549067f4b8a11e3c9f3245046b1cd491e54a698ec1a3f9ff419b19b70edcd3657fd22e63f4778c5028c92458b336f586c63c0b60dfb3425526ae340fef4485b0ffcc8f7192e1578d48b831585b77738f276fc165d2b1b0bd101a3a7463533c3b84621a3d67c5a20449912bb3cdd413d47bc66becdd9463a4c11740578563d96c8e119ced706a6b9114d328d13b71f6e65a74b517f5f5f7f977165b082ac6bc98f940dac825c57d4323d9606c2aa10af8639b85a7df6f81d921df22bdfc717006299f5d384d17ff50bb0c7d7e476c821b963f34a6276c8dc24a8abeaafd903bb51df352880d2b11b7878ad8cac9f8c3adbbeb41e53c72f3fbc5238b29489617f46b7af4b00b6fa9a975331fd715ef252bb9c9587584003d722d7b67a7323d1ee74272d2f74982477ba09715e77f3beb7b6d156cee2dbd6077567710b1bccbe2eb71e7deea3695a520575df28f3beddced7de1b8cab08b771531229ff654cc18da04ea3d6679fb4a36227a34709993a533717b10db28517cb8d20ccc715a3b5e8985e49512311ef7b32ad2ce9eb9b2970adb6015a058b947bc042e2f92e61ae31e31ea2e805c33c63bdb181b88e2538eb7fcc6d31943c28f532d954fa66d002982e15d1b1cbbcbfd75093f1c011fa7df85b5fb516e2ca102f6dd61813c8ec144a639a3b13c4bdc11454dfbf7c86c1ece0c807888c2043bbf01abdef95275209c387842ce615a3d95fd6fca7fe501f5ff792d1169aef4ded2fa7a7edbbd5b42f9ef2b97c3faf005324204e8c68a25a9bd315b1be7c1962313863377b67387a817c6b43d253476d345f87b530be4b1672ecf7762cfcfe56b65569c5ec5c217c4b9083900b53bb6798df4e7c720bca0f7632f5b3ca1e3aa16db5101c991fc320c71ce43da97f112c8fb89337d9409c5fe2da943b600bc05fbaef5a0596ad18e78097f58880d32dd0cb96fadb707398e3a655994c346f5a1908d42c95eec8cc5ecf5603ef72ab836a4245b108f9d0fdaf663a8598d89ada8a5dfc9223c906daceae073ecfe4d4635a4ff35c582f384f4d8e761a3a2d0ca6ee94e8b333326795b811df8425894b0d1cf0d24347d20be3bad080aa2914a0aa3d0f07631a072a65cee5b16f00a07ec71b64a8b9927c2c004b2e2063a3a1003c8a27a9bbd9b0ad8d473e98740e9f6b1713b08435dbaa905773841af86ab772426720308f4696e6435976cdbd83782f15f9d38100bc09f99c7c29d282836fa01da56768831bd61a9d3db398aca28667c6412906fc5e8ad49ae3860f8e51cf707785e1d3e08ca80bfa295cb42d0c2d89acc9b58fea37d0741888e393cac23840899611ca51052a40599245086c9a6b874fc3ce279fbd14f79356a25971d89230925de9414662a8a3e614e8c90922d8bf1761774ef3e5ed076b684326154ea080cd5cda28aa9e46e964130a65559c2a3ca586c03670a56a499882c2d215477ce0307bb44029c04ba1eda35625b7f68e06731e62c3ecd2a3eff3e4b2bf64b47160f6807473ab78a54d483bc758528b28e452ff0b8d373e14e0798f95ee6912f6d0f3e8c4b7ca99b784558622cf78a17415a129b4cff5cd8cd881c2c96c557a8450261be9a4f3d20d325afc6a333b96b005e6ca3c338a426aafe2d3b73c0841628084a0438e8142c0a86ca888691ccbee56fe3a20932d59065a5083b62d3a30aa30c5ac7c301ec2d5fee7b07c9a12fd12da63d7d8610befc202f2b7ddf155672ce008c3adaaf83757b472ce332c7df2e0191ec12094b17f09c7b006edbda7355fe022924222caab36eb54e55516ba65a94d7191c0508513b2365e452305d1c21a209576f2fb7b28baadfad058d311f69a7c10e40045650aa79632b4da9b8cad33c093f95bf670d06eaa139b335a613286e95c856e1474c1f3252701de49adc0e0500bf2ab2538a77971d9c7653c12b89053b2c62bfa29ce67ac894ec5ab27a72fc0c9cdc4502944760e8e74d1d391fc6ea1593f976307d49c183cffaa96f71267d0b769418c84f6af0165aa438425ec961cfc1c2038aa96e8b2c361676538954e001fad157057946511f13a7d45df1bd2c9175672d2184eadefa331c8445c75b9209134030436b2034f465831102a94681eb7ac8ba4c208c3c0404abf59fa4cad20660871821a06fd6bfc3137722e07e8abc302804d615d85e0fdd2330613bb2f302d89a9b9dc058d3ad863ffa677d6288fb2caaa5d0122d7ad66250b9973cd2db0f2ebbdea831ff11c5c3214c62f0a7dcace8f64961a0a291beef808607c20b5c06ef2f133604c8bdd2c2966e1ab2da2741e9d78e193e8d91aa97d255160531dc008c2ddc28c2103467804b7ce79aaf58b2ca5a3bb3b346a0a83ec5801615d79f0e9118c1170b0cdc7224c5fd8c85c9648f06efd23f40bbc9b8018c2a2f78a3710f1d479827149b9ef50c6547a439224e18101eac2b0c61a9234eeec7f52dbd990d2e6afc0ed06cd10e733a67140ff56cba4c7b6ba6796a1753f351d832f052fd0127e3ff48f1a526b93a62efe5a7d5d4c6080174a4d1c96f95452a3ba19de1140a"};
// tx hash 1640236fe817f83b0bd2082c655d152be390e4f78e41db5b935bb8a53249fe8c

TEST_F(MYSQL_TEST, DeleteExistingTxAndAssociatedOutputsAndInputs)
{
    // removing a tx should automatically remove its outputs and inputs
    // from mysql. we have constrans doing this, so we check here if
    // they work as we think they do

    TX_AND_ACC_FROM_HEX(tx_1640_hex, addr_57H_hex)

    xmreg::XmrTransaction tx_data;

    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    uint64_t no_of_deleted_rows = xmr_accounts->delete_tx(tx_data.id.data);

    ASSERT_EQ(no_of_deleted_rows, 1);

    // this tx has 1 output and two key images.
    //
    // output: a9d876b01eb972db944b78899b4c90c2b66a3c81fe04bf54ff61565f3db53419
    // key1: 45d3cf7b4f5db9d614602e9e956c63c35cf3da6f7b93740c514e06c898fb0da2
    // key2: 3ef54482411c54a280081a87fd35e542b3b8906b702123c55468e71dd620d3a4

    // so we expect those to be also deleted after tx has been deleted


    vector<xmreg::XmrOutput> outputs;

    bool is_success = xmr_accounts->select_for_tx(tx_data.id.data, outputs);

    // if no outputs found, we return false
    EXPECT_FALSE(is_success);
    EXPECT_EQ(outputs.size(), 0);

    vector<xmreg::XmrInput> inputs;

    is_success = xmr_accounts->select_for_tx(tx_data.id.data, inputs);

    // if no inputs found, we return false
    EXPECT_FALSE(is_success);
    EXPECT_EQ(inputs.size(), 0);
}

TEST_F(MYSQL_TEST, DeleteNoNExistingTx)
{
    uint64_t some_non_existing_tx_id = 7774748483;

    uint64_t no_of_deleted_rows = xmr_accounts->delete_tx(some_non_existing_tx_id);

    EXPECT_EQ(no_of_deleted_rows, 0);
}


TEST_F(MYSQL_TEST, MarkTxSpendableAndNonSpendable)
{
    TX_AND_ACC_FROM_HEX(tx_fc4_hex, owner_addr_5Ajfk);

    xmreg::XmrTransaction tx_data;


    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    // this particular tx is marked as spendable in mysql
    EXPECT_TRUE(static_cast<bool>(tx_data.spendable));

    uint64_t no_of_changed_rows = xmr_accounts->mark_tx_nonspendable(tx_data.id.data);

    EXPECT_EQ(no_of_changed_rows, 1);

    // fetch tx_data again and check if its not-spendable now
    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    EXPECT_FALSE(static_cast<bool>(tx_data.spendable));

    // mark it as spendable
    no_of_changed_rows = xmr_accounts->mark_tx_spendable(tx_data.id.data);

    EXPECT_EQ(no_of_changed_rows, 1);

    // fetch it again, and check if its spendable
    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    EXPECT_TRUE(static_cast<bool>(tx_data.spendable));

    // try ding same but when disconnected
    xmr_accounts->disconnect();
    EXPECT_EQ(xmr_accounts->mark_tx_spendable(tx_data.id.data), 0);
}

TEST_F(MYSQL_TEST, GetTotalRecievedByAnAddressWhenDisconnected)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    uint64_t total_recieved;

    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->get_total_recieved(acc.id.data, total_recieved));
}

// now we can test total number of incoming xmr (only outputs)
// for serveral addresses. For this we can use
// parametrized fixture

                                          // addr, expected recieved xmr
using address_incoming_balance = std::pair<string, uint64_t>;

class MYSQL_TEST_PARAM :
        public MYSQL_TEST,
        public ::testing::WithParamInterface<address_incoming_balance>
{};

TEST_P(MYSQL_TEST_PARAM, GetTotalRecievedByAnAddress)
{
    auto const& own_addr          = GetParam().first;
    auto const& expected_recieved = GetParam().second;

    ACC_FROM_HEX(own_addr);

    uint64_t total_recieved;

    EXPECT_TRUE(xmr_accounts->get_total_recieved(acc.id.data, total_recieved));

    EXPECT_EQ(total_recieved, expected_recieved);
}

INSTANTIATE_TEST_CASE_P(
        RecivedXmrTest, MYSQL_TEST_PARAM,
                ::testing::Values(
                        make_pair(owner_addr_5Ajfk, 697348926585540ull),
                        make_pair(addr_55Zb       , 1046498996045077ull)
                ));


auto
make_mock_output_data(string last_char_pub_key = "4")
{
    xmreg::XmrOutput mock_output_data ;

    mock_output_data.id           = mysqlpp::null;
    // mock_output_data.account_id   = acc.id; need to be set when used
    mock_output_data.tx_id        = 106086; // some tx id for this output
    mock_output_data.out_pub_key  = "18c6a80311d6f455ac1e5abdce7e86828d1ecf911f78da12a56ce8fdd5c716f"
                                    + last_char_pub_key; // out_pub_key is unique field, so to make
                                                         // few public keys, we just change its last char.
    mock_output_data.tx_pub_key   = "38ae1d790bce890c3750b20ba8d35b8edee439fc8fb4218d50cec39a0cb7844a";
    mock_output_data.amount       = 999916984840000ull;
    mock_output_data.out_index    = 1;
    mock_output_data.rct_outpk    = "e17cdc23fac1d92f2de196b567c8dd55ecd4cac52d6fef4eb446b6de4edb1d01";
    mock_output_data.rct_mask     = "03cea1ffc18193639f7432287432c058a70551ceebed0db2c9d18088b423a255";
    mock_output_data.rct_amount   = "f02e6d9dd504e6b428170d37b79344cad5538a4ad32f3f7dcebd5b96ac522e07";
    mock_output_data.global_index = 64916;
    mock_output_data.mixin        = 7;
    mock_output_data.timestamp    = mysqlpp::DateTime(static_cast<time_t>(44434554));;

    return mock_output_data;
}

TEST_F(MYSQL_TEST, SelectOutputsForAccount)
{
    // select all outputs associated with the given account

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrOutput> outputs;

    ASSERT_TRUE(xmr_accounts->select(acc.id.data, outputs));

    // can use this command to get list of outputs
    // ./xmr2csv --stagenet -m -a 5AjfkEY7RFgNGDYvoRQkncfwHXT6Fh7oJBisqFUX5u96i3ZepxDPocQK29tmAwBDuvKRpskZnfA6N8Ra58qFzA4bSA3QZFp -v 3ee772709dcf834a881c432fc15625d84585e4462d7905c42460dd478a315008 -t 90000 -g 101610
    EXPECT_EQ(outputs.size(), 9);
}

TEST_F(MYSQL_TEST, SelectOutputsForTransaction)
{
    // select all outputs associated with given transaction

    // tx hash 1640236fe817f83b0bd2082c655d152be390e4f78e41db5b935bb8a53249fe8c
    TX_AND_ACC_FROM_HEX(tx_1640_hex, addr_57H_hex)

    xmreg::XmrTransaction tx_data;

    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    vector<xmreg::XmrOutput> outputs;

    bool is_success = xmr_accounts->select_for_tx(tx_data.id.data, outputs);

    ASSERT_TRUE(is_success);

    EXPECT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].out_pub_key, "a9d876b01eb972db944b78899b4c90c2b66a3c81fe04bf54ff61565f3db53419");


    // now use output_exists

    xmreg::XmrOutput out;

    EXPECT_TRUE(xmr_accounts->output_exists(outputs[0].out_pub_key, out));

    EXPECT_EQ(outputs[0], out);

    // use output_exists on non-exisiting output

    string non_exist_key {"a9d876b01eb972db944b78899b4c90c2b66a3c81fe04bf54ff61565f3db53000"};

    EXPECT_FALSE(xmr_accounts->output_exists(non_exist_key, out));

    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->output_exists(non_exist_key, out));

}

TEST_F(MYSQL_TEST, InsertOneOutput)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    xmreg::XmrOutput mock_output_data = make_mock_output_data();

    mock_output_data.account_id = acc.id.data;

    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(mock_output_data);
    uint64_t inserted_output_id = xmr_accounts->insert(mock_output_data);

    EXPECT_EQ(inserted_output_id, expected_primary_id);

    // now we fetch the inserted output and compare its values

    xmreg::XmrOutput out_data2;

    EXPECT_TRUE(xmr_accounts->select_by_primary_id(inserted_output_id, out_data2));

    EXPECT_EQ(out_data2.tx_id, mock_output_data.tx_id);
    EXPECT_EQ(out_data2.out_pub_key, mock_output_data.out_pub_key);
    EXPECT_EQ(out_data2.tx_pub_key, mock_output_data.tx_pub_key);
    EXPECT_EQ(out_data2.amount, mock_output_data.amount);
    EXPECT_EQ(out_data2.out_index, mock_output_data.out_index);
    EXPECT_EQ(out_data2.rct_outpk, mock_output_data.rct_outpk);
    EXPECT_EQ(out_data2.rct_mask, mock_output_data.rct_mask);
    EXPECT_EQ(out_data2.rct_amount, mock_output_data.rct_amount);
    EXPECT_EQ(out_data2.global_index, mock_output_data.global_index);
    EXPECT_EQ(out_data2.mixin, mock_output_data.mixin);
    EXPECT_EQ(out_data2.timestamp, mock_output_data.timestamp);

    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->select_by_primary_id(inserted_output_id, out_data2));
}


TEST_F(MYSQL_TEST, TryToInsertSameOutputTwice)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    xmreg::XmrOutput mock_output_data = make_mock_output_data();

    mock_output_data.account_id = acc.id.data;

    // first time insert should be fine
    uint64_t inserted_output_id = xmr_accounts->insert(mock_output_data);

    EXPECT_GT(inserted_output_id, 0);

    // second insert should fail and result in 0
    inserted_output_id = xmr_accounts->insert(mock_output_data);

    EXPECT_EQ(inserted_output_id, 0);
}

TEST_F(MYSQL_TEST, TryToInsertSameOutputTwiceToDifferent)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    xmreg::XmrOutput mock_output_data = make_mock_output_data();

    mock_output_data.account_id = acc.id.data;

    // first time insert should be fine
    uint64_t inserted_output_id = xmr_accounts->insert(mock_output_data);

    EXPECT_GT(inserted_output_id, 0);

    xmreg::XmrAccount acc2;                                                 \
    ASSERT_TRUE(xmr_accounts->select(addr_55Zb, acc2));

    mock_output_data.account_id = acc2.id.data;

    // second insert should fail and result in 0
    inserted_output_id = xmr_accounts->insert(mock_output_data);

    EXPECT_EQ(inserted_output_id, 0);
}

TEST_F(MYSQL_TEST, InsertSeverlOutputsAtOnce)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    // create vector of several outputs to be written into database
    vector<xmreg::XmrOutput> mock_outputs_data;

    for (size_t i = 0; i < 10; ++i)
    {
        mock_outputs_data.push_back(make_mock_output_data(std::to_string(i)));
        mock_outputs_data.back().account_id = acc.id.data;
    }

    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(xmreg::XmrOutput());

    // first time insert should be fine
    uint64_t no_inserted_rows = xmr_accounts->insert(mock_outputs_data);

    EXPECT_EQ(no_inserted_rows, mock_outputs_data.size());

    // after inserting 10 rows, the expected ID should be before + 11
    uint64_t expected_primary_id2 = xmr_accounts->get_next_primary_id(xmreg::XmrOutput());

    EXPECT_EQ(expected_primary_id2, expected_primary_id + mock_outputs_data.size());

    for (size_t i = 0; i < 10; ++i)
    {
        uint64_t output_id_to_get = expected_primary_id + i;

        xmreg::XmrOutput out_data;
        xmr_accounts->select_by_primary_id(output_id_to_get, out_data);

        EXPECT_EQ(mock_outputs_data[i].out_pub_key, out_data.out_pub_key);
    }

    xmr_accounts->disconnect();
    EXPECT_EQ(xmr_accounts->insert(mock_outputs_data), 0);
}


TEST_F(MYSQL_TEST, SelectInputsForAccount)
{
    // select all inputs associated with the given account

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrInput> inputs;

    ASSERT_TRUE(xmr_accounts->select(acc.id.data, inputs));

    // can use this command to get the list of ring members
    // ./xmr2csv --stagenet -m -a 5AjfkEY7RFgNGDYvoRQkncfwHXT6Fh7oJBisqFUX5u96i3ZepxDPocQK29tmAwBDuvKRpskZnfA6N8Ra58qFzA4bSA3QZFp -v 3ee772709dcf834a881c432fc15625d84585e4462d7905c42460dd478a315008 -t 90000 -g 101610
    EXPECT_EQ(inputs.size(), 12);
}

TEST_F(MYSQL_TEST, SelectInputsForTransaction)
{
    // select all outputs associated with given transaction

    // tx hash 1640236fe817f83b0bd2082c655d152be390e4f78e41db5b935bb8a53249fe8c
    TX_AND_ACC_FROM_HEX(tx_1640_hex, addr_57H_hex)

    xmreg::XmrTransaction tx_data;

    ASSERT_TRUE(xmr_accounts->tx_exists(acc.id.data, tx_hash_str, tx_data));

    vector<xmreg::XmrInput> inputs;

    bool is_success = xmr_accounts->select_for_tx(tx_data.id.data, inputs);

    ASSERT_TRUE(is_success);

    EXPECT_EQ(inputs.size(), 2);
    EXPECT_EQ(inputs[0].key_image, "45d3cf7b4f5db9d614602e9e956c63c35cf3da6f7b93740c514e06c898fb0da2");
    EXPECT_EQ(inputs[1].key_image, "3ef54482411c54a280081a87fd35e542b3b8906b702123c55468e71dd620d3a4");
}


TEST_F(MYSQL_TEST, SelectInputsForOutput)
{
    // select all inputs associated with given output
    // i.e., where the output is used as a ring member

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrOutput> outputs;

    ASSERT_TRUE(xmr_accounts->select(acc.id.data, outputs));


    // we check only for the first output
    // for second output we should have three key images
    // out pub key: 7f03cbcf4f9ddc763543959f0152bb5e66147359b6097bccf8fa0bbd748e1445
    uint64_t output_id = outputs.at(1).id.data;

    vector<xmreg::XmrInput> inputs;

    ASSERT_TRUE(xmr_accounts->select_inputs_for_out(output_id, inputs));

    EXPECT_EQ(inputs.size(), 3);

    EXPECT_EQ(inputs.front().key_image, "00dd88b3a16b3616d342faec2bc47b24add433407ef79b9a00b55b75d96239a4");
    EXPECT_EQ(inputs.back().key_image, "abc529357f90641d501d5108f822617049c19461569eafa45cb5400ee45bef33");

    inputs.clear();

    // for non exisitng output
    ASSERT_FALSE(xmr_accounts->select_inputs_for_out(444444, inputs));

    // try doing same when disconnected
    xmr_accounts->disconnect();

    EXPECT_FALSE(xmr_accounts->select_inputs_for_out(output_id, inputs));
}



auto
make_mock_input_data(string last_char_pub_key = "0")
{
    xmreg::XmrInput mock_data;

    mock_data.id           = mysqlpp::null;
    // mock_output_data.account_id   = acc.id; need to be set when used
    mock_data.tx_id        = 106086; // some tx id for this output
    mock_data.output_id    = 428900; // some output id
    mock_data.key_image    = "18c6a80311d6f455ac1e5abdce7e86828d1ecf911f78da12a56ce8fdd5c716f"
                                    + last_char_pub_key; // out_pub_key is unique field, so to make
    // few public keys, we just change its last char.
    mock_data.amount       = 999916984840000ull;
    mock_data.timestamp    = mysqlpp::DateTime(static_cast<time_t>(44434554));;

    return mock_data;
}


TEST_F(MYSQL_TEST, InsertOneInput)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    xmreg::XmrInput mock_input_data = make_mock_input_data();

    mock_input_data.account_id = acc.id.data;

    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(mock_input_data);
    uint64_t inserted_id  = xmr_accounts->insert(mock_input_data);

    EXPECT_EQ(expected_primary_id, inserted_id);

    // now we fetch the inserted input and compare its values

    xmreg::XmrInput in_data2;

    EXPECT_TRUE(xmr_accounts->select_by_primary_id(inserted_id, in_data2));

    EXPECT_EQ(in_data2.account_id, mock_input_data.account_id);
    EXPECT_EQ(in_data2.tx_id, mock_input_data.tx_id);
    EXPECT_EQ(in_data2.amount, mock_input_data.amount);
    EXPECT_EQ(in_data2.key_image, mock_input_data.key_image);
    EXPECT_EQ(in_data2.output_id, mock_input_data.output_id);
    EXPECT_EQ(in_data2.timestamp, mock_input_data.timestamp);
}



TEST_F(MYSQL_TEST, InsertSeverlInputsAtOnce)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    // create vector of several inputs to be written into database
    vector<xmreg::XmrInput> mock_data;

    for (size_t i = 0; i < 10; ++i)
    {
        mock_data.push_back(make_mock_input_data(std::to_string(i)));
        mock_data.back().account_id = acc.id.data;
    }

    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(xmreg::XmrInput());

    // first time insert should be fine
    uint64_t no_inserted_rows = xmr_accounts->insert(mock_data);

    EXPECT_EQ(no_inserted_rows, mock_data.size());

    // after inserting 10 rows, the expected ID should be before + 11
    uint64_t expected_primary_id2 = xmr_accounts->get_next_primary_id(xmreg::XmrInput());

    EXPECT_EQ(expected_primary_id2, expected_primary_id + mock_data.size());

    for (size_t i = 0; i < 10; ++i)
    {
        uint64_t id_to_get = expected_primary_id + i;

        xmreg::XmrInput out_data;
        xmr_accounts->select_by_primary_id(id_to_get, out_data);

        EXPECT_EQ(mock_data[i].key_image, out_data.key_image);
    }

}


TEST_F(MYSQL_TEST, TryToInsertSameInputTwice)
{
    // the input table requires a row to be unique for pair
    // of key_image + output_id

    ACC_FROM_HEX(owner_addr_5Ajfk);

    xmreg::XmrInput mock_data = make_mock_input_data();

    mock_data.account_id = acc.id.data;

    // first time insert should be fine
    uint64_t inserted_id = xmr_accounts->insert(mock_data);

    EXPECT_GT(inserted_id, 0);

    // second insert should fail and result in 0
    inserted_id = xmr_accounts->insert(mock_data);

    EXPECT_EQ(inserted_id, 0);
}


TEST_F(MYSQL_TEST, SelectPaymentForAccount)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrPayment> payments;

    ASSERT_TRUE(xmr_accounts->select(acc.id.data, payments));

    EXPECT_EQ(payments.size(), 1);
    EXPECT_EQ(payments[0].payment_id, "74854c1cd490e148");
    EXPECT_EQ(payments[0].payment_address, "5DUWE29P72Eb8inMa41HuNJG4tj9CcaNKGr6EVSbvhWGJdpDQCiNNYBUNF1oDb8BczU5aD68d3HNKXaEsPq8cvbQLK4Tiiy");
    EXPECT_FALSE(static_cast<bool>(payments[0].request_fulfilled));

    //check if there is no payment for the given account id
    EXPECT_FALSE(xmr_accounts->select(5555, payments));
}

TEST_F(MYSQL_TEST, SelectPaymentUsingPaymentID)
{
    string exising_id {"e410eb43e14a28fb"};

    vector<xmreg::XmrPayment> payments;

    EXPECT_TRUE(xmr_accounts->select_payment_by_id(exising_id, payments));

    EXPECT_EQ(payments.size(), 1);


    string non_exising_id {"e410eb43e140000"};

    EXPECT_FALSE(xmr_accounts->select_payment_by_id(non_exising_id, payments));

    EXPECT_EQ(payments.size(), 0);

    xmr_accounts->disconnect();
    EXPECT_FALSE(xmr_accounts->select_payment_by_id(exising_id, payments));
}

auto
make_mock_payment_data(string last_char_pub_key = "0")
{
    xmreg::XmrPayment mock_data;

    mock_data.id                = mysqlpp::null;
    // mock_output_data.account_id   = acc.id; need to be set when used
    mock_data.payment_id        = pod_to_hex(crypto::rand<crypto::hash8>());
    mock_data.import_fee        = 10000000010ull; // xmr
    mock_data.request_fulfilled = false;
    mock_data.tx_hash           = ""; // no tx_hash yet with the payment
    mock_data.payment_address   = "5DUWE29P72Eb8inMa41HuNJG4tj9CcaNKGr6EVSbvhWGJdpDQCiNNYBUNF1oDb8BczU5aD68d3HNKXaEsPq8cvbQLK4Tiiy";

    return mock_data;
}


TEST_F(MYSQL_TEST, InsertOnePayment)
{
    ACC_FROM_HEX(addr_55Zb);

    xmreg::XmrPayment mock_data = make_mock_payment_data();

    mock_data.account_id = acc.id.data;

    uint64_t expected_primary_id = xmr_accounts->get_next_primary_id(mock_data);
    uint64_t inserted_id  = xmr_accounts->insert(mock_data);

    EXPECT_EQ(expected_primary_id, inserted_id);

    // now we fetch the inserted input and compare its values

    xmreg::XmrPayment in_data2;

    EXPECT_TRUE(xmr_accounts->select_by_primary_id(inserted_id, in_data2));

    EXPECT_EQ(in_data2.account_id, mock_data.account_id);
    EXPECT_EQ(in_data2.payment_id, mock_data.payment_id);
    EXPECT_EQ(in_data2.import_fee, mock_data.import_fee);
    EXPECT_EQ(in_data2.payment_address, mock_data.payment_address);
    EXPECT_EQ(in_data2.request_fulfilled, mock_data.request_fulfilled);
}


TEST_F(MYSQL_TEST, TryToInsertSamePaymentTwice)
{
    // the input table requires a row to be unique for pair
    // of key_image + output_id

    ACC_FROM_HEX(addr_55Zb);

    xmreg::XmrPayment mock_data = make_mock_payment_data();

    mock_data.account_id = acc.id.data;

    uint64_t inserted_id  = xmr_accounts->insert(mock_data);

    EXPECT_GT(inserted_id, 0);

    // second insert should fail and result in 0
    inserted_id = xmr_accounts->insert(mock_data);

    EXPECT_EQ(inserted_id, 0);
}


TEST_F(MYSQL_TEST, UpdatePayment)
{
    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrPayment> payments;

    ASSERT_TRUE(xmr_accounts->select(acc.id.data, payments));


    // make copy of the orginal payment so that we can update it easly
    auto updated_payment = payments.at(0);


    ASSERT_FALSE(static_cast<bool>(updated_payment.request_fulfilled));
    ASSERT_EQ(updated_payment.tx_hash, "");


    updated_payment.request_fulfilled = 1;
    updated_payment.tx_hash = "7456e8f14cc327ac653fa680e20d53e43b61be238302ea4589bb5a73932e921c";

    EXPECT_TRUE(xmr_accounts->update( payments.at(0), updated_payment));

    // fetch the payment data and check if it was updated
    vector<xmreg::XmrPayment> payments2;

    ASSERT_TRUE(xmr_accounts->select(acc.id.data, payments2));

    EXPECT_EQ(payments2.at(0).id.data, payments.at(0).id.data);
    EXPECT_TRUE(static_cast<bool>(payments2.at(0).request_fulfilled));
    EXPECT_EQ(payments2.at(0).tx_hash, updated_payment.tx_hash);
}

/*
 * Dont want to use real blockchain data, so we are going to
 * mock xmreg::CurrentBlockchainStatus
 */
class MockCurrentBlockchainStatus1 : public xmreg::CurrentBlockchainStatus
{
public:
    MockCurrentBlockchainStatus1()
            : xmreg::CurrentBlockchainStatus(
                  xmreg::BlockchainSetup(),
                  nullptr)
    {}

    bool tx_unlock_state {true};
    bool tx_exist_state {true};

    std::map<string, uint64_t> tx_exist_mock_data;

    // all txs in the blockchain are unlocked
    virtual bool
    is_tx_unlocked(uint64_t unlock_time, uint64_t block_height) override
    {
        return tx_unlock_state;
    }

    // all ts in the blockchain exists
    virtual bool
    tx_exist(const string& tx_hash_str, uint64_t& tx_index) override
    {
        if (tx_exist_mock_data.empty())
            return tx_exist_state;

        tx_index = tx_exist_mock_data[tx_hash_str];

        return true;
    }
};

TEST_F(MYSQL_TEST, SelectTxsIfAllAreSpendableAndExist)
{
    // if all txs selected for the given account are spendable
    // the select_txs_for_account_spendability_check method
    // should not change anything

    // use mock CurrentBlockchainStatus instead of real object
    // which would access the real blockchain.
    auto mock_bc_status = make_shared<MockCurrentBlockchainStatus1>();

    xmr_accounts->set_bc_status_provider(mock_bc_status);

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrTransaction> txs;

    // we mark all txs for this account as spendable
    // so that we have something to work with in this test
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    auto no_of_original_txs = txs.size();

    for (size_t i = 0; i < txs.size(); ++i)
        this->xmr_accounts->mark_tx_spendable(txs[i].id.data);

    // reselect tx after they were marked as spendable
    txs.clear();
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    EXPECT_TRUE(this->xmr_accounts->select_txs_for_account_spendability_check(acc.id.data, txs));

    // we check if non of the input txs got filtere out
    EXPECT_EQ(txs.size(), no_of_original_txs);

    // and we check if all of remained spendable
    for (auto const& tx: txs)
        EXPECT_TRUE(bool {tx.spendable});
}



TEST_F(MYSQL_TEST, SelectTxsIfAllAreNonspendableButUnlockedAndExist)
{
    // if all txs selected for the given account are non-spendable
    // the select_txs_for_account_spendability_check method
    // will check their unlock time, and marked them spendable in
    // they are unlocked.
    // We are going to mock that all are unlocked

    // use mock CurrentBlockchainStatus instead of real object
    // which would access the real blockchain.
    auto mock_bc_status = make_shared<MockCurrentBlockchainStatus1>();

    xmr_accounts->set_bc_status_provider(mock_bc_status);

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrTransaction> txs;

    // we mark all txs for this account as spendable
    // so that we have something to work with in this test
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    auto no_of_original_txs = txs.size();

    for (size_t i = 0; i < txs.size(); ++i)
        this->xmr_accounts->mark_tx_nonspendable(txs[i].id.data);

    // reselect tx after they were marked as spendable
    txs.clear();
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    // and we check if all non-spendable before we fetch them into
    // select_txs_for_account_spendability_check
    for (auto const& tx: txs)
        ASSERT_FALSE(bool {tx.spendable});

    EXPECT_TRUE(this->xmr_accounts->select_txs_for_account_spendability_check(acc.id.data, txs));

    // we check if non of the input txs got filtere out
    EXPECT_EQ(txs.size(), no_of_original_txs);


    // reselect tx after they were marked as spendable
    txs.clear();
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    // and we check if all of remained spendable
    for (auto const& tx: txs)
        ASSERT_TRUE(bool {tx.spendable});

    // and we check if all of remained spendable
    for (auto const& tx: txs)
        EXPECT_TRUE(bool {tx.spendable});
}



TEST_F(MYSQL_TEST, SelectTxsIfAllAreNonspendableLockedButExist)
{
    // if all txs selected for the given account are non-spendable
    // the select_txs_for_account_spendability_check method
    // will check if they are unlocked. In this thest all will be locked
    // so new unlock_time for them is going to be set for them.
    // all txs are set to exisit in the blockchain

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrTransaction> txs;

    // we mark all txs for this account as spendable
    // so that we have something to work with in this test
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    // use mock CurrentBlockchainStatus instead of real object
    // which would access the real blockchain.
    auto mock_bc_status = make_shared<MockCurrentBlockchainStatus1>();

    // we mock that all txs are still locked and cant be spent
    mock_bc_status->tx_unlock_state = false;

    // now we need to populate mock_bc_status->tx_exist_mock_data map
    // so that tx_exist mock works as if all txs existed
    // and it returns correct/expected blockchain_tx_id
    for(auto const& tx: txs)
        mock_bc_status->tx_exist_mock_data[tx.hash] = tx.blockchain_tx_id;

    // now set the mock_bc_status to be used by xmr_accounts later on
    xmr_accounts->set_bc_status_provider(mock_bc_status);

    auto no_of_original_txs = txs.size();

    for (size_t i = 0; i < txs.size(); ++i)
        this->xmr_accounts->mark_tx_nonspendable(txs[i].id.data);

    // reselect tx after they were marked as spendable
    txs.clear();
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    // and we check if all non-spendable before we fetch them into
    // select_txs_for_account_spendability_check
    for (auto const& tx: txs)
    {
        ASSERT_FALSE(bool {tx.spendable});
        if (!bool {tx.coinbase})
            ASSERT_EQ(tx.unlock_time, 0);
    }

    EXPECT_TRUE(this->xmr_accounts->select_txs_for_account_spendability_check(acc.id.data, txs));

    // we check if non of the input txs got filtere out
    EXPECT_EQ(txs.size(), no_of_original_txs);

    // and we check if all of remained non-spendable and
    // if their unlock time was modified as needed
    for (auto const& tx: txs)
    {
        ASSERT_FALSE(bool {tx.spendable});
        if (!bool {tx.coinbase})
            ASSERT_EQ(tx.unlock_time, tx.height + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE);
    }
}


TEST_F(MYSQL_TEST, SelectTxsIfAllAreNonspendableUnlockedAndDontExist)
{
    // if all txs selected for the given account are non-spendable,
    // locked and dont exist in blockchain
    // they should be filtered and removed from the mysql

    // use mock CurrentBlockchainStatus instead of real object
    // which would access the real blockchain.
    auto mock_bc_status = make_shared<MockCurrentBlockchainStatus1>();

    // all txs are locked and dont exisit in blockchain
    mock_bc_status->tx_unlock_state = false;
    mock_bc_status->tx_exist_state = false;

    xmr_accounts->set_bc_status_provider(mock_bc_status);

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrTransaction> txs;

    // we mark all txs for this account as nonspendable
    // so that we have something to work with in this test
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    auto no_of_original_txs = txs.size();

    for (size_t i = 0; i < txs.size(); ++i)
        this->xmr_accounts->mark_tx_nonspendable(txs[i].id.data);

    // reselect tx after they were marked as spendable
    txs.clear();
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    // and we check if all non-spendable before we fetch them into
    // select_txs_for_account_spendability_check
    for (auto const& tx: txs)
        ASSERT_FALSE(bool {tx.spendable});

    EXPECT_TRUE(this->xmr_accounts->select_txs_for_account_spendability_check(acc.id.data, txs));

    // after the call to select_txs_for_account_spendability_check
    // all txs should be filted out
    EXPECT_EQ(txs.size(), 0);

    // all these txs should also be deleted from the mysql
    txs.clear();
    ASSERT_FALSE(this->xmr_accounts->select(acc.id.data, txs));
    EXPECT_EQ(txs.size(), 0);
}



TEST_F(MYSQL_TEST, SelectTxsFailureDueToSpendabilityAndTxDeletion)
{
    // if all txs selected for the given account are non-spendable,
    // locked and dont exist in blockchain
    // they should be filtered and removed from the mysql
    // however, the mark_tx_spendable and delete_tx can
    // fail. We simulate the failure by providing tx.id which does not
    // exisit in the mysql

    // use mock CurrentBlockchainStatus instead of real object
    // which would access the real blockchain.
    auto mock_bc_status = make_shared<MockCurrentBlockchainStatus1>();

    // all txs are unlocked and dont exisit in blockchain
    mock_bc_status->tx_unlock_state = true;
    mock_bc_status->tx_exist_state = false;

    xmr_accounts->set_bc_status_provider(mock_bc_status);

    ACC_FROM_HEX(owner_addr_5Ajfk);

    vector<xmreg::XmrTransaction> txs;

    // we mark all txs for this account as nonspendable
    // so that we have something to work with in this test
    ASSERT_TRUE(this->xmr_accounts->select(acc.id.data, txs));

    auto no_of_original_txs = txs.size();

    for (size_t i = 0; i < txs.size(); ++i)
    {
        txs[i].id = 5555555555 /*some non-existing id*/;
        txs[i].spendable = false;
    }

    // the non-exisiting ids should result in failure
    EXPECT_FALSE(this->xmr_accounts->select_txs_for_account_spendability_check(acc.id.data, txs));

    // now repeat if all txs are locked and dont exisit in blockchain
    mock_bc_status->tx_unlock_state = false;

    // also should lead to false
    EXPECT_FALSE(this->xmr_accounts->select_txs_for_account_spendability_check(acc.id.data, txs));

}


TEST_F(MYSQL_TEST, MysqlPingThreadStopsOnPingFailure)
{
    // we test creation of the mysql ping thread
    // and its stop due to ping failure, for example,
    // when connection to mysql is lost

    auto conn = xmr_accounts->get_connection();

    // create ping functor that will be pinging mysql every 1 second
    xmreg::MysqlPing mysql_ping {conn, 1};

    {
        // create ping thread and start pinging

        // we put it in this local scope so that
        // we exit the scope only when the thread is fully finished
        // i.e., it was joined.
        xmreg::ThreadRAII mysql_ping_thread(
                std::thread(std::ref(mysql_ping)),
                xmreg::ThreadRAII::DtorAction::join);

        // wait few seconds so that several pings are done
        std::this_thread::sleep_for(4s);

        // disconnecting from mysql should result in the thread to stop
        conn->get_connection().disconnect();
    }

    // since we waiting 4s, we expect that there were at about 3-4 pings
    EXPECT_EQ(mysql_ping.get_stop_reason(), xmreg::MysqlPing::StopReason::PingFailed);
    EXPECT_THAT(mysql_ping.get_counter(), AllOf(Ge(3), Le(5)));
}

TEST_F(MYSQL_TEST, MysqlPingThreadStopsOnPointerExpiry)
{
    // we test creation of the mysql ping thread
    // and weak pointer in the ping object expires.

    // to simulate the pointer expiry, we are going to create new connection here
    // i.e., so we dont use the one from the fixture, as its shared pointer
    // will be keeping the weak pointer in the functor alive

    // we did specify wrong mysql details so this should throw.

    auto new_conn = make_connection();

    ASSERT_TRUE(new_conn->get_connection().connected());

    // create ping functor that will be pinging mysql every 1 second
    xmreg::MysqlPing mysql_ping {new_conn, 1};

    {
        // create ping thread and start pinging

        // we put it in this local scope so that
        // we exit the scope only when the thread is fully finished
        // i.e., it was joined.
        xmreg::ThreadRAII mysql_ping_thread(
                std::thread(std::ref(mysql_ping)),
                xmreg::ThreadRAII::DtorAction::join);

        // wait few seconds so that several pings are done
        std::this_thread::sleep_for(4s);

        // reset new_conn pointer. this should lead to expiry of
        // weak pointer in mysql_ping function
        new_conn.reset();
    }

    // since we waiting 4s, we expect that there were at about 3-4 pings
    EXPECT_EQ(mysql_ping.get_stop_reason(), xmreg::MysqlPing::StopReason::PointerExpired);
    EXPECT_THAT(mysql_ping.get_counter(), AllOf(Ge(3), Le(5)));
}


//class MYSQL_TEST2 : public MYSQL_TEST
//{
//
//};


//TEST(TEST_CHAIN, GenerateTestChain)
//{
//    uint64_t ts_start = 1338224400;
//
//    std::vector<test_event_entry> events;
//
//    GENERATE_ACCOUNT(miner);
//    GENERATE_ACCOUNT(alice);
//
//    MAKE_GENESIS_BLOCK(events, blk_0, miner, ts_start);
//    MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner);
//    MAKE_NEXT_BLOCK(events, blk_1_side, blk_0, miner);
//    MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner);
//
//    std::vector<cryptonote::block> chain;
//    map_hash2tx_t mtx;
//
//
//    REWIND_BLOCKS(events, blk_2r, blk_2, miner);
//    MAKE_TX_LIST_START(events, txlist_0, miner, alice, MK_COINS(1), blk_2);
//    MAKE_TX_LIST(events, txlist_0, miner, alice, MK_COINS(2), blk_2);
//    MAKE_TX_LIST(events, txlist_0, miner, alice, MK_COINS(4), blk_2);
//    MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner, txlist_0);
//    REWIND_BLOCKS(events, blk_3r, blk_3, miner);
//    MAKE_TX(events, tx_1, miner, alice, MK_COINS(50), blk_3);
//    MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3r, miner, tx_1);
//    REWIND_BLOCKS(events, blk_4r, blk_4, miner);
//    MAKE_TX(events, tx_2, miner, alice, MK_COINS(50), blk_4);
//    MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4r, miner, tx_2);
//    REWIND_BLOCKS(events, blk_5r, blk_5, miner);
//    MAKE_TX(events, tx_3, miner, alice, MK_COINS(50), blk_5);
//    MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5r, miner, tx_3);
//    REWIND_BLOCKS(events, blk_6r, blk_6, miner);
//    MAKE_TX(events, tx_4, alice, miner, MK_COINS(20), blk_6);
//    MAKE_NEXT_BLOCK_TX1(events, blk_7, blk_6r, miner, tx_4);
//
//    find_block_chain(events, chain, mtx, get_block_hash(blk_7));
//
//
//    std::cout << "Alice BALANCE = " << get_balance(alice, chain, mtx) << std::endl;
//
//    cout << get_account_address_as_str(network_type::MAINNET, false, alice.get_keys().m_account_address) << endl;
//    cout << pod_to_hex(alice.get_keys().m_view_secret_key) << endl;
//    cout << pod_to_hex(alice.get_keys().m_spend_secret_key) << endl;
//
//    cout << '\n\n' << xmreg::tx_to_hex(tx_4) << '\n';
//
//}

}
