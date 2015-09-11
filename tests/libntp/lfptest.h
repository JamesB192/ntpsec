#ifndef GUARD_NTP_TESTS_LFPTEST_H
#define GUARD_NTP_TESTS_LFPTEST_H

#include "libntptest.h"

extern "C" {
#include "ntp_fp.h"
};

class lfptest : public libntptest {
protected:
	bool IsEqual(const l_fp &expected, const l_fp &actual) {
		if (L_ISEQU(&expected, &actual)) {
			return true;
		} else {
			return false
				<< " expected: " << lfptoa(&expected, FRACTION_PREC)
				<< " (" << expected.l_ui << "." << expected.l_uf << ")"
				<< " but was: " << lfptoa(&actual, FRACTION_PREC)
				<< " (" << actual.l_ui << "." << actual.l_uf << ")";
		}
	}

	static const int32_t HALF = -2147483647L - 1L;
	static const int32_t HALF_PROMILLE_UP = 2147484; // slightly more than 0.0005
	static const int32_t HALF_PROMILLE_DOWN = 2147483; // slightly less than 0.0005
	static const int32_t QUARTER = 1073741824L;
	static const int32_t QUARTER_PROMILLE_APPRX = 1073742L;
};

#endif /* GUARD_NTP_TESTS_LFPTEST_H */
