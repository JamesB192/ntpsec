extern "C" {
#include "unity.h"
#include "unity_fixture.h"
}

#include <iostream>

TEST_GROUP(keyFile);

TEST_SETUP(keyFile) {}

TEST_TEAR_DOWN(keyFile) {}

#include "fileHandlingTest.h"

extern "C" {
#include "crypto.h"
};

class keyFileTest : public fileHandlingTest {
protected:
	bool CompareKeys(key& expected, key& actual) {
		if (expected.key_id != actual.key_id) {
			std::cout << "Expected key_id: " << expected.key_id
				<< " but was: " << actual.key_id << std::endl;
			return false;
		}
		if (expected.key_len != actual.key_len) {
			std::cout << "Expected key_len: " << expected.key_len
				<< " but was: " << actual.key_len << std::endl;
			return false;
		}
		if (strcmp(expected.type, actual.type) != 0) {
			std::cout << "Expected key_type: " << expected.type
				<< " but was: " << actual.type << std::endl;
			return false;
		}
		if (memcmp(expected.key_seq, actual.key_seq, expected.key_len) != 0) {
			std::cout << "Key mismatch!" << std::endl;
			return false;
		}
		return true;
	}

	bool CompareKeys(int key_id,
					       int key_len,
					       const char* type,
					       const char* key_seq,
					       key& actual) {
		key temp;

		temp.key_id = key_id;
		temp.key_len = key_len;
		strlcpy(temp.type, type, sizeof(temp.type));
		memcpy(temp.key_seq, key_seq, key_len);

		return CompareKeys(temp, actual);
	}
};

TEST(keyFile, ReadEmptyKeyFile) {
	key* keys = NULL;

	TEST_ASSERT_EQUAL(0, auth_init(CreatePath("key-test-empty", INPUT_DIR).c_str(), &keys));

	TEST_ASSERT_TRUE(keys == NULL);
}

TEST(keyFile, ReadASCIIKeys) {
	key* keys = NULL;

	TEST_ASSERT_EQUAL(2, auth_init(CreatePath("key-test-ascii", INPUT_DIR).c_str(), &keys));

	TEST_ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(40, &result);
	TEST_ASSERT_TRUE(result != NULL);
	TEST_ASSERT_TRUE(CompareKeys(40, 11, "MD5", "asciikeyTwo", *result));

	result = NULL;
	get_key(50, &result);
	TEST_ASSERT_TRUE(result != NULL);
	TEST_ASSERT_TRUE(CompareKeys(50, 11, "MD5", "asciikeyOne", *result));
}

TEST(keyFile, ReadHexKeys) {
	key* keys = NULL;

	TEST_ASSERT_EQUAL(3, auth_init(CreatePath("key-test-hex", INPUT_DIR).c_str(), &keys));

	TEST_ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(10, &result);
	TEST_ASSERT_TRUE(result != NULL);
	TEST_ASSERT_TRUE(CompareKeys(10, 13, "MD5",
		 "\x01\x23\x45\x67\x89\xab\xcd\xef\x01\x23\x45\x67\x89", *result));

	result = NULL;
	get_key(20, &result);
	TEST_ASSERT_TRUE(result != NULL);
	char data1[15]; memset(data1, 0x11, 15);
	TEST_ASSERT_TRUE(CompareKeys(20, 15, "MD5", data1, *result));

	result = NULL;
	get_key(30, &result);
	TEST_ASSERT_TRUE(result != NULL);
	char data2[13]; memset(data2, 0x01, 13);
	TEST_ASSERT_TRUE(CompareKeys(30, 13, "MD5", data2, *result));
}

TEST(keyFile, ReadKeyFileWithComments) {
	key* keys = NULL;

	TEST_ASSERT_EQUAL(2, auth_init(CreatePath("key-test-comments", INPUT_DIR).c_str(), &keys));

	TEST_ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(10, &result);
	TEST_ASSERT_TRUE(result != NULL);
	char data[15]; memset(data, 0x01, 15);
	TEST_ASSERT_TRUE(CompareKeys(10, 15, "MD5", data, *result));

	result = NULL;
	get_key(34, &result);
	TEST_ASSERT_TRUE(result != NULL);
	TEST_ASSERT_TRUE(CompareKeys(34, 3, "MD5", "xyz", *result));
}

TEST(keyFile, ReadKeyFileWithInvalidHex) {
	key* keys = NULL;

	TEST_ASSERT_EQUAL(1, auth_init(CreatePath("key-test-invalid-hex", INPUT_DIR).c_str(), &keys));

	TEST_ASSERT_TRUE(keys != NULL);

	key* result = NULL;
	get_key(10, &result);
	TEST_ASSERT_TRUE(result != NULL);
	char data[15]; memset(data, 0x01, 15);
	TEST_ASSERT_TRUE(CompareKeys(10, 15, "MD5", data, *result));

	result = NULL;
	get_key(30, &result); // Should not exist, and result should remain NULL.
	TEST_ASSERT_TRUE(result == NULL);
}
