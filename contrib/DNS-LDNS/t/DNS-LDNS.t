# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl DNS-LDNS.t'

#########################

# change 'tests => 2' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 2;
BEGIN { use_ok('DNS::LDNS') };


my $fail = 0;
foreach my $constname (qw(
	LDNS_AA LDNS_AD LDNS_CD LDNS_CERT_ACPKIX LDNS_CERT_IACPKIX
	LDNS_CERT_IPGP LDNS_CERT_IPKIX LDNS_CERT_ISPKI LDNS_CERT_OID
	LDNS_CERT_PGP LDNS_CERT_PKIX LDNS_CERT_SPKI LDNS_CERT_URI
	LDNS_DEFAULT_TTL LDNS_DH LDNS_DSA LDNS_DSA_NSEC3 LDNS_ECC LDNS_ECC_GOST
	LDNS_HASH_GOST LDNS_IP4ADDRLEN LDNS_IP6ADDRLEN
	LDNS_KEY_REVOKE_KEY LDNS_KEY_SEP_KEY LDNS_KEY_ZONE_KEY
	LDNS_MAX_DOMAINLEN LDNS_MAX_LABELLEN LDNS_MAX_PACKETLEN
	LDNS_MAX_POINTERS LDNS_MAX_RDFLEN LDNS_NSEC3_VARS_OPTOUT_MASK
	LDNS_PACKET_ANSWER LDNS_PACKET_IQUERY LDNS_PACKET_NODATA
	LDNS_PACKET_NOTIFY LDNS_PACKET_NXDOMAIN LDNS_PACKET_QUERY
	LDNS_PACKET_QUESTION LDNS_PACKET_REFERRAL LDNS_PACKET_STATUS
	LDNS_PACKET_UNKNOWN LDNS_PACKET_UPDATE LDNS_PORT LDNS_PRIVATEDNS
	LDNS_PRIVATEOID LDNS_QR LDNS_RA LDNS_RCODE_FORMERR LDNS_RCODE_NOERROR
	LDNS_RCODE_NOTAUTH LDNS_RCODE_NOTIMPL LDNS_RCODE_NOTZONE
	LDNS_RCODE_NXDOMAIN LDNS_RCODE_NXRRSET LDNS_RCODE_REFUSED
	LDNS_RCODE_SERVFAIL LDNS_RCODE_YXDOMAIN LDNS_RCODE_YXRRSET LDNS_RD
	LDNS_RDATA_FIELD_DESCRIPTORS_COMMON LDNS_RDF_SIZE_16BYTES
	LDNS_RDF_SIZE_6BYTES LDNS_RDF_SIZE_BYTE LDNS_RDF_SIZE_DOUBLEWORD
	LDNS_RDF_SIZE_WORD LDNS_RDF_TYPE_A LDNS_RDF_TYPE_AAAA LDNS_RDF_TYPE_ALG
	LDNS_RDF_TYPE_APL LDNS_RDF_TYPE_ATMA LDNS_RDF_TYPE_B32_EXT
	LDNS_RDF_TYPE_B64 LDNS_RDF_TYPE_CERT_ALG LDNS_RDF_TYPE_CLASS
	LDNS_RDF_TYPE_DNAME LDNS_RDF_TYPE_HEX LDNS_RDF_TYPE_INT16
	LDNS_RDF_TYPE_INT16_DATA LDNS_RDF_TYPE_INT32 LDNS_RDF_TYPE_INT8
	LDNS_RDF_TYPE_IPSECKEY LDNS_RDF_TYPE_LOC LDNS_RDF_TYPE_NONE
	LDNS_RDF_TYPE_NSAP LDNS_RDF_TYPE_NSEC LDNS_RDF_TYPE_NSEC3_NEXT_OWNER
	LDNS_RDF_TYPE_NSEC3_SALT LDNS_RDF_TYPE_PERIOD LDNS_RDF_TYPE_SERVICE
	LDNS_RDF_TYPE_STR LDNS_RDF_TYPE_TIME LDNS_RDF_TYPE_HIP
	LDNS_RDF_TYPE_TSIGTIME LDNS_RDF_TYPE_TYPE LDNS_RDF_TYPE_UNKNOWN
	LDNS_RDF_TYPE_WKS LDNS_RESOLV_ANCHOR LDNS_RESOLV_DEFDOMAIN
	LDNS_RESOLV_INET LDNS_RESOLV_INET6 LDNS_RESOLV_INETANY
	LDNS_RESOLV_KEYWORD LDNS_RESOLV_KEYWORDS LDNS_RESOLV_NAMESERVER
	LDNS_RESOLV_OPTIONS LDNS_RESOLV_RTT_INF LDNS_RESOLV_RTT_MIN
	LDNS_RESOLV_SEARCH LDNS_RESOLV_SORTLIST LDNS_RR_CLASS_ANY
	LDNS_RR_CLASS_CH LDNS_RR_CLASS_COUNT LDNS_RR_CLASS_FIRST
	LDNS_RR_CLASS_HS LDNS_RR_CLASS_IN LDNS_RR_CLASS_LAST LDNS_RR_CLASS_NONE
	LDNS_RR_COMPRESS LDNS_RR_NO_COMPRESS LDNS_RR_OVERHEAD LDNS_RR_TYPE_A
	LDNS_RR_TYPE_A6 LDNS_RR_TYPE_AAAA LDNS_RR_TYPE_AFSDB LDNS_RR_TYPE_ANY
	LDNS_RR_TYPE_APL LDNS_RR_TYPE_ATMA LDNS_RR_TYPE_AXFR LDNS_RR_TYPE_CERT
	LDNS_RR_TYPE_CNAME LDNS_RR_TYPE_COUNT LDNS_RR_TYPE_DHCID
	LDNS_RR_TYPE_DLV LDNS_RR_TYPE_DNAME LDNS_RR_TYPE_DNSKEY LDNS_RR_TYPE_DS
	LDNS_RR_TYPE_EID LDNS_RR_TYPE_FIRST LDNS_RR_TYPE_GID LDNS_RR_TYPE_GPOS
	LDNS_RR_TYPE_HINFO LDNS_RR_TYPE_IPSECKEY LDNS_RR_TYPE_ISDN
	LDNS_RR_TYPE_IXFR LDNS_RR_TYPE_KEY LDNS_RR_TYPE_KX LDNS_RR_TYPE_LAST
	LDNS_RR_TYPE_LOC LDNS_RR_TYPE_MAILA LDNS_RR_TYPE_MAILB LDNS_RR_TYPE_MB
	LDNS_RR_TYPE_MD LDNS_RR_TYPE_MF LDNS_RR_TYPE_MG LDNS_RR_TYPE_MINFO
	LDNS_RR_TYPE_MR LDNS_RR_TYPE_MX LDNS_RR_TYPE_NAPTR LDNS_RR_TYPE_NIMLOC
	LDNS_RR_TYPE_NS LDNS_RR_TYPE_NSAP LDNS_RR_TYPE_NSAP_PTR
	LDNS_RR_TYPE_NSEC LDNS_RR_TYPE_NSEC3 LDNS_RR_TYPE_NSEC3PARAM
	LDNS_RR_TYPE_NSEC3PARAMS LDNS_RR_TYPE_NULL LDNS_RR_TYPE_NXT
	LDNS_RR_TYPE_OPT LDNS_RR_TYPE_PTR LDNS_RR_TYPE_PX LDNS_RR_TYPE_RP
	LDNS_RR_TYPE_RRSIG LDNS_RR_TYPE_RT LDNS_RR_TYPE_SIG LDNS_RR_TYPE_SINK
	LDNS_RR_TYPE_SOA LDNS_RR_TYPE_SPF LDNS_RR_TYPE_SRV LDNS_RR_TYPE_SSHFP
	LDNS_RR_TYPE_TALINK LDNS_RR_TYPE_TSIG LDNS_RR_TYPE_TXT LDNS_RR_TYPE_UID
	LDNS_RR_TYPE_UINFO LDNS_RR_TYPE_UNSPEC LDNS_RR_TYPE_WKS
	LDNS_RR_TYPE_X25 LDNS_RSAMD5 LDNS_RSASHA1 LDNS_RSASHA1_NSEC3
	LDNS_RSASHA256 LDNS_RSASHA512 LDNS_SECTION_ADDITIONAL
	LDNS_SECTION_ANSWER LDNS_SECTION_ANY LDNS_SECTION_ANY_NOQUESTION
	LDNS_SECTION_AUTHORITY LDNS_SECTION_QUESTION LDNS_SHA1 LDNS_SHA256
	LDNS_SIGN_DSA LDNS_SIGN_DSA_NSEC3 LDNS_SIGN_ECC_GOST
	LDNS_SIGN_HMACSHA1 LDNS_SIGN_HMACSHA256
	LDNS_SIGN_RSAMD5 LDNS_SIGN_RSASHA1 LDNS_SIGN_RSASHA1_NSEC3
	LDNS_SIGN_RSASHA256 LDNS_SIGN_RSASHA512 LDNS_STATUS_ADDRESS_ERR
	LDNS_STATUS_CERT_BAD_ALGORITHM LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL
	LDNS_STATUS_CRYPTO_BOGUS LDNS_STATUS_CRYPTO_EXPIRATION_BEFORE_INCEPTION
	LDNS_STATUS_CRYPTO_NO_DNSKEY LDNS_STATUS_CRYPTO_NO_DS
	LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY
	LDNS_STATUS_CRYPTO_NO_RRSIG LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY
	LDNS_STATUS_CRYPTO_NO_TRUSTED_DS LDNS_STATUS_CRYPTO_SIG_EXPIRED
	LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED LDNS_STATUS_CRYPTO_TSIG_BOGUS
	LDNS_STATUS_CRYPTO_TSIG_ERR LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR
	LDNS_STATUS_CRYPTO_UNKNOWN_ALGO LDNS_STATUS_CRYPTO_VALIDATED
	LDNS_STATUS_DDD_OVERFLOW LDNS_STATUS_DNSSEC_EXISTENCE_DENIED
	LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND
	LDNS_STATUS_DNSSEC_NSEC_RR_NOT_COVERED
	LDNS_STATUS_DNSSEC_NSEC_WILDCARD_NOT_COVERED
	LDNS_STATUS_DOMAINNAME_OVERFLOW LDNS_STATUS_DOMAINNAME_UNDERFLOW
	LDNS_STATUS_EMPTY_LABEL LDNS_STATUS_ENGINE_KEY_NOT_LOADED
	LDNS_STATUS_ERR LDNS_STATUS_FILE_ERR LDNS_STATUS_INTERNAL_ERR
	LDNS_STATUS_INVALID_B32_EXT LDNS_STATUS_INVALID_B64
	LDNS_STATUS_INVALID_HEX LDNS_STATUS_INVALID_INT LDNS_STATUS_INVALID_IP4
	LDNS_STATUS_INVALID_IP6 LDNS_STATUS_INVALID_POINTER
	LDNS_STATUS_INVALID_STR LDNS_STATUS_INVALID_TIME
	LDNS_STATUS_LABEL_OVERFLOW LDNS_STATUS_MEM_ERR
	LDNS_STATUS_MISSING_RDATA_FIELDS_KEY
	LDNS_STATUS_MISSING_RDATA_FIELDS_RRSIG LDNS_STATUS_NETWORK_ERR
	LDNS_STATUS_NOT_IMPL LDNS_STATUS_NO_DATA LDNS_STATUS_NSEC3_ERR
	LDNS_STATUS_NULL LDNS_STATUS_OK LDNS_STATUS_PACKET_OVERFLOW
	LDNS_STATUS_RES_NO_NS LDNS_STATUS_RES_QUERY LDNS_STATUS_SOCKET_ERROR
	LDNS_STATUS_SSL_ERR LDNS_STATUS_SYNTAX_ALG_ERR
	LDNS_STATUS_SYNTAX_BAD_ESCAPE LDNS_STATUS_SYNTAX_CLASS_ERR
	LDNS_STATUS_SYNTAX_DNAME_ERR LDNS_STATUS_SYNTAX_EMPTY
	LDNS_STATUS_SYNTAX_ERR LDNS_STATUS_SYNTAX_INCLUDE
	LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL
	LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW
	LDNS_STATUS_SYNTAX_ITERATIONS_OVERFLOW LDNS_STATUS_SYNTAX_KEYWORD_ERR
	LDNS_STATUS_SYNTAX_MISSING_VALUE_ERR LDNS_STATUS_SYNTAX_ORIGIN
	LDNS_STATUS_SYNTAX_RDATA_ERR LDNS_STATUS_SYNTAX_TTL
	LDNS_STATUS_SYNTAX_TTL_ERR LDNS_STATUS_SYNTAX_TYPE_ERR
	LDNS_STATUS_SYNTAX_VERSION_ERR LDNS_STATUS_UNKNOWN_INET
	LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL
	LDNS_STATUS_WIRE_INCOMPLETE_ANSWER
	LDNS_STATUS_WIRE_INCOMPLETE_AUTHORITY
	LDNS_STATUS_WIRE_INCOMPLETE_HEADER LDNS_STATUS_WIRE_INCOMPLETE_QUESTION
	LDNS_TC)) {
  next if (eval "my \$a = $constname; 1");
  if ($@ =~ /^Your vendor has not defined LDNS macro $constname/) {
    print "# pass: $@";
  } else {
    print "# fail: $@";
    $fail = 1;
  }

}

ok( $fail == 0 , 'Constants' );
#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

