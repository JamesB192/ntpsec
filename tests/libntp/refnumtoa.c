extern "C" {
#include "unity.h"
#include "unity_fixture.h"
}

TEST_GROUP(refnumtoa);

TEST_SETUP(refnumtoa) {}

TEST_TEAR_DOWN(refnumtoa) {}

#include "libntptest.h"

#include "ntp_net.h"
#include "ntp_refclock.h"

#include <sstream>

class refnumtoaTest : public libntptest {
protected:
	/* Might need to be updated if a new refclock gets this id. */
	static const int UNUSED_REFCLOCK_ID = 250;
};

#ifdef REFCLOCK		/* clockname() is useless otherwise */
TEST(refnumtoa, LocalClock) {
	/* We test with a refclock address of type LOCALCLOCK.
	 * with id 8
	 */
	u_int32 addr = REFCLOCK_ADDR;
	addr |= REFCLK_LOCALCLOCK << 8;
	addr |= 0x8;

	sockaddr_u address;
	address.sa4.sin_family = AF_INET;
	address.sa4.sin_addr.s_addr = htonl(addr);

	std::ostringstream expected;
	expected << clockname(REFCLK_LOCALCLOCK)
			 << "(8)";

	TEST_ASSERT_EQUAL_STRING(expected.str().c_str(), refnumtoa(&address));
}
#endif	/* REFCLOCK */

#ifdef REFCLOCK		/* refnumtoa() is useless otherwise */
TEST(refnumtoa, UnknownId) {
	/* We test with a currently unused refclock ID */
	u_int32 addr = REFCLOCK_ADDR;
	addr |= UNUSED_REFCLOCK_ID << 8;
	addr |= 0x4;

	sockaddr_u address;
	address.sa4.sin_family = AF_INET;
	address.sa4.sin_addr.s_addr = htonl(addr);

	std::ostringstream expected;
	expected << "REFCLK(" << UNUSED_REFCLOCK_ID
			 << ",4)";

	TEST_ASSERT_EQUAL_STRING(expected.str().c_str(), refnumtoa(&address));
}
#endif	/* REFCLOCK */

TEST_GROUP_RUNNER(refnumtoa) {
	RUN_TEST_CASE(refnumtoa, LocalClock);
	RUN_TEST_CASE(refnumtoa, UnknownId);
}
