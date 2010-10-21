/*
 *   fs/cifs/cifsencrypt.c
 *
 *   Copyright (C) International Business Machines  Corp., 2005,2006
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifs_debug.h"
#include "md5.h"
#include "cifs_unicode.h"
#include "cifsproto.h"
#include "ntlmssp.h"
#include <linux/ctype.h>
#include <linux/random.h>

/* Calculate and return the CIFS signature based on the mac key and SMB PDU */
/* the 16 byte signature must be allocated by the caller  */
/* Note we only use the 1st eight bytes */
/* Note that the smb header signature field on input contains the
	sequence number before this function is called */

extern void mdfour(unsigned char *out, unsigned char *in, int n);
extern void E_md4hash(const unsigned char *passwd, unsigned char *p16);
extern void SMBencrypt(unsigned char *passwd, const unsigned char *c8,
		       unsigned char *p24);

static int cifs_calculate_signature(const struct smb_hdr *cifs_pdu,
				struct TCP_Server_Info *server, char *signature)
{
	struct	MD5Context context;

	if (cifs_pdu == NULL || signature == NULL || server == NULL)
		return -EINVAL;

	cifs_MD5_init(&context);
	cifs_MD5_update(&context, server->session_key.response,
			server->session_key.len);
	cifs_MD5_update(&context, cifs_pdu->Protocol, cifs_pdu->smb_buf_length);

	cifs_MD5_final(signature, &context);
	return 0;
}

int cifs_sign_smb(struct smb_hdr *cifs_pdu, struct TCP_Server_Info *server,
		  __u32 *pexpected_response_sequence_number)
{
	int rc = 0;
	char smb_signature[20];

	if ((cifs_pdu == NULL) || (server == NULL))
		return -EINVAL;

	if ((cifs_pdu->Flags2 & SMBFLG2_SECURITY_SIGNATURE) == 0)
		return rc;

	spin_lock(&GlobalMid_Lock);
	cifs_pdu->Signature.Sequence.SequenceNumber =
			cpu_to_le32(server->sequence_number);
	cifs_pdu->Signature.Sequence.Reserved = 0;

	*pexpected_response_sequence_number = server->sequence_number++;
	server->sequence_number++;
	spin_unlock(&GlobalMid_Lock);

	rc = cifs_calculate_signature(cifs_pdu, server, smb_signature);
	if (rc)
		memset(cifs_pdu->Signature.SecuritySignature, 0, 8);
	else
		memcpy(cifs_pdu->Signature.SecuritySignature, smb_signature, 8);

	return rc;
}

static int cifs_calc_signature2(const struct kvec *iov, int n_vec,
				struct TCP_Server_Info *server, char *signature)
{
	struct  MD5Context context;
	int i;

	if (iov == NULL || signature == NULL || server == NULL)
		return -EINVAL;

	cifs_MD5_init(&context);
	cifs_MD5_update(&context, server->session_key.response,
				server->session_key.len);
	for (i = 0; i < n_vec; i++) {
		if (iov[i].iov_len == 0)
			continue;
		if (iov[i].iov_base == NULL) {
			cERROR(1, "null iovec entry");
			return -EIO;
		}
		/* The first entry includes a length field (which does not get
		   signed that occupies the first 4 bytes before the header */
		if (i == 0) {
			if (iov[0].iov_len <= 8) /* cmd field at offset 9 */
				break; /* nothing to sign or corrupt header */
			cifs_MD5_update(&context, iov[0].iov_base+4,
				  iov[0].iov_len-4);
		} else
			cifs_MD5_update(&context, iov[i].iov_base, iov[i].iov_len);
	}

	cifs_MD5_final(signature, &context);

	return 0;
}


int cifs_sign_smb2(struct kvec *iov, int n_vec, struct TCP_Server_Info *server,
		   __u32 *pexpected_response_sequence_number)
{
	int rc = 0;
	char smb_signature[20];
	struct smb_hdr *cifs_pdu = iov[0].iov_base;

	if ((cifs_pdu == NULL) || (server == NULL))
		return -EINVAL;

	if ((cifs_pdu->Flags2 & SMBFLG2_SECURITY_SIGNATURE) == 0)
		return rc;

	spin_lock(&GlobalMid_Lock);
	cifs_pdu->Signature.Sequence.SequenceNumber =
				cpu_to_le32(server->sequence_number);
	cifs_pdu->Signature.Sequence.Reserved = 0;

	*pexpected_response_sequence_number = server->sequence_number++;
	server->sequence_number++;
	spin_unlock(&GlobalMid_Lock);

	rc = cifs_calc_signature2(iov, n_vec, server, smb_signature);
	if (rc)
		memset(cifs_pdu->Signature.SecuritySignature, 0, 8);
	else
		memcpy(cifs_pdu->Signature.SecuritySignature, smb_signature, 8);

	return rc;
}

int cifs_verify_signature(struct smb_hdr *cifs_pdu,
			  struct TCP_Server_Info *server,
			  __u32 expected_sequence_number)
{
	unsigned int rc;
	char server_response_sig[8];
	char what_we_think_sig_should_be[20];

	if (cifs_pdu == NULL || server == NULL)
		return -EINVAL;

	if (cifs_pdu->Command == SMB_COM_NEGOTIATE)
		return 0;

	if (cifs_pdu->Command == SMB_COM_LOCKING_ANDX) {
		struct smb_com_lock_req *pSMB =
			(struct smb_com_lock_req *)cifs_pdu;
	    if (pSMB->LockType & LOCKING_ANDX_OPLOCK_RELEASE)
			return 0;
	}

	/* BB what if signatures are supposed to be on for session but
	   server does not send one? BB */

	/* Do not need to verify session setups with signature "BSRSPYL "  */
	if (memcmp(cifs_pdu->Signature.SecuritySignature, "BSRSPYL ", 8) == 0)
		cFYI(1, "dummy signature received for smb command 0x%x",
			cifs_pdu->Command);

	/* save off the origiginal signature so we can modify the smb and check
		its signature against what the server sent */
	memcpy(server_response_sig, cifs_pdu->Signature.SecuritySignature, 8);

	cifs_pdu->Signature.Sequence.SequenceNumber =
					cpu_to_le32(expected_sequence_number);
	cifs_pdu->Signature.Sequence.Reserved = 0;

	rc = cifs_calculate_signature(cifs_pdu, server,
		what_we_think_sig_should_be);

	if (rc)
		return rc;

/*	cifs_dump_mem("what we think it should be: ",
		      what_we_think_sig_should_be, 16); */

	if (memcmp(server_response_sig, what_we_think_sig_should_be, 8))
		return -EACCES;
	else
		return 0;

}

/* first calculate 24 bytes ntlm response and then 16 byte session key */
int setup_ntlm_response(struct cifsSesInfo *ses)
{
	unsigned int temp_len = CIFS_SESS_KEY_SIZE + CIFS_AUTH_RESP_SIZE;
	char temp_key[CIFS_SESS_KEY_SIZE];

	if (!ses)
		return -EINVAL;

	ses->auth_key.response = kmalloc(temp_len, GFP_KERNEL);
	if (!ses->auth_key.response) {
		cERROR(1, "NTLM can't allocate (%u bytes) memory", temp_len);
		return -ENOMEM;
	}
	ses->auth_key.len = temp_len;

	SMBNTencrypt(ses->password, ses->cryptKey,
			ses->auth_key.response + CIFS_SESS_KEY_SIZE);

	E_md4hash(ses->password, temp_key);
	mdfour(ses->auth_key.response, temp_key, CIFS_SESS_KEY_SIZE);

	return 0;
}

#ifdef CONFIG_CIFS_WEAK_PW_HASH
void calc_lanman_hash(const char *password, const char *cryptkey, bool encrypt,
			char *lnm_session_key)
{
	int i;
	char password_with_pad[CIFS_ENCPWD_SIZE];

	memset(password_with_pad, 0, CIFS_ENCPWD_SIZE);
	if (password)
		strncpy(password_with_pad, password, CIFS_ENCPWD_SIZE);

	if (!encrypt && global_secflags & CIFSSEC_MAY_PLNTXT) {
		memset(lnm_session_key, 0, CIFS_SESS_KEY_SIZE);
		memcpy(lnm_session_key, password_with_pad,
			CIFS_ENCPWD_SIZE);
		return;
	}

	/* calculate old style session key */
	/* calling toupper is less broken than repeatedly
	calling nls_toupper would be since that will never
	work for UTF8, but neither handles multibyte code pages
	but the only alternative would be converting to UCS-16 (Unicode)
	(using a routine something like UniStrupr) then
	uppercasing and then converting back from Unicode - which
	would only worth doing it if we knew it were utf8. Basically
	utf8 and other multibyte codepages each need their own strupper
	function since a byte at a time will ont work. */

	for (i = 0; i < CIFS_ENCPWD_SIZE; i++)
		password_with_pad[i] = toupper(password_with_pad[i]);

	SMBencrypt(password_with_pad, cryptkey, lnm_session_key);

	/* clear password before we return/free memory */
	memset(password_with_pad, 0, CIFS_ENCPWD_SIZE);
}
#endif /* CIFS_WEAK_PW_HASH */

/* Build a proper attribute value/target info pairs blob.
 * Fill in netbios and dns domain name and workstation name
 * and client time (total five av pairs and + one end of fields indicator.
 * Allocate domain name which gets freed when session struct is deallocated.
 */
static int
build_avpair_blob(struct cifsSesInfo *ses, const struct nls_table *nls_cp)
{
	unsigned int dlen;
	unsigned int wlen;
	unsigned int size = 6 * sizeof(struct ntlmssp2_name);
	__le64  curtime;
	char *defdmname = "WORKGROUP";
	unsigned char *blobptr;
	struct ntlmssp2_name *attrptr;

	if (!ses->domainName) {
		ses->domainName = kstrdup(defdmname, GFP_KERNEL);
		if (!ses->domainName)
			return -ENOMEM;
	}

	dlen = strlen(ses->domainName);
	wlen = strlen(ses->server->hostname);

	/* The length of this blob is a size which is
	 * six times the size of a structure which holds name/size +
	 * two times the unicode length of a domain name +
	 * two times the unicode length of a server name +
	 * size of a timestamp (which is 8 bytes).
	 */
	ses->tilen = size + 2 * (2 * dlen) + 2 * (2 * wlen) + 8;
	ses->tiblob = kzalloc(ses->tilen, GFP_KERNEL);
	if (!ses->tiblob) {
		ses->tilen = 0;
		cERROR(1, "Challenge target info allocation failure");
		return -ENOMEM;
	}

	blobptr = ses->tiblob;
	attrptr = (struct ntlmssp2_name *) blobptr;

	attrptr->type = cpu_to_le16(NTLMSSP_AV_NB_DOMAIN_NAME);
	attrptr->length = cpu_to_le16(2 * dlen);
	blobptr = (unsigned char *)attrptr + sizeof(struct ntlmssp2_name);
	cifs_strtoUCS((__le16 *)blobptr, ses->domainName, dlen, nls_cp);

	blobptr += 2 * dlen;
	attrptr = (struct ntlmssp2_name *) blobptr;

	attrptr->type = cpu_to_le16(NTLMSSP_AV_NB_COMPUTER_NAME);
	attrptr->length = cpu_to_le16(2 * wlen);
	blobptr = (unsigned char *)attrptr + sizeof(struct ntlmssp2_name);
	cifs_strtoUCS((__le16 *)blobptr, ses->server->hostname, wlen, nls_cp);

	blobptr += 2 * wlen;
	attrptr = (struct ntlmssp2_name *) blobptr;

	attrptr->type = cpu_to_le16(NTLMSSP_AV_DNS_DOMAIN_NAME);
	attrptr->length = cpu_to_le16(2 * dlen);
	blobptr = (unsigned char *)attrptr + sizeof(struct ntlmssp2_name);
	cifs_strtoUCS((__le16 *)blobptr, ses->domainName, dlen, nls_cp);

	blobptr += 2 * dlen;
	attrptr = (struct ntlmssp2_name *) blobptr;

	attrptr->type = cpu_to_le16(NTLMSSP_AV_DNS_COMPUTER_NAME);
	attrptr->length = cpu_to_le16(2 * wlen);
	blobptr = (unsigned char *)attrptr + sizeof(struct ntlmssp2_name);
	cifs_strtoUCS((__le16 *)blobptr, ses->server->hostname, wlen, nls_cp);

	blobptr += 2 * wlen;
	attrptr = (struct ntlmssp2_name *) blobptr;

	attrptr->type = cpu_to_le16(NTLMSSP_AV_TIMESTAMP);
	attrptr->length = cpu_to_le16(sizeof(__le64));
	blobptr = (unsigned char *)attrptr + sizeof(struct ntlmssp2_name);
	curtime = cpu_to_le64(cifs_UnixTimeToNT(CURRENT_TIME));
	memcpy(blobptr, &curtime, sizeof(__le64));

	return 0;
}

/* Server has provided av pairs/target info in the type 2 challenge
 * packet and we have plucked it and stored within smb session.
 * We parse that blob here to find netbios domain name to be used
 * as part of ntlmv2 authentication (in Target String), if not already
 * specified on the command line.
 * If this function returns without any error but without fetching
 * domain name, authentication may fail against some server but
 * may not fail against other (those who are not very particular
 * about target string i.e. for some, just user name might suffice.
 */
static int
find_domain_name(struct cifsSesInfo *ses)
{
	unsigned int attrsize;
	unsigned int type;
	unsigned int onesize = sizeof(struct ntlmssp2_name);
	unsigned char *blobptr;
	unsigned char *blobend;
	struct ntlmssp2_name *attrptr;

	if (!ses->tilen || !ses->tiblob)
		return 0;

	blobptr = ses->tiblob;
	blobend = ses->tiblob + ses->tilen;

	while (blobptr + onesize < blobend) {
		attrptr = (struct ntlmssp2_name *) blobptr;
		type = le16_to_cpu(attrptr->type);
		if (type == NTLMSSP_AV_EOL)
			break;
		blobptr += 2; /* advance attr type */
		attrsize = le16_to_cpu(attrptr->length);
		blobptr += 2; /* advance attr size */
		if (blobptr + attrsize > blobend)
			break;
		if (type == NTLMSSP_AV_NB_DOMAIN_NAME) {
			if (!attrsize)
				break;
			if (!ses->domainName) {
				struct nls_table *default_nls;
				ses->domainName =
					kmalloc(attrsize + 1, GFP_KERNEL);
				if (!ses->domainName)
						return -ENOMEM;
				default_nls = load_nls_default();
				cifs_from_ucs2(ses->domainName,
					(__le16 *)blobptr, attrsize, attrsize,
					default_nls, false);
				unload_nls(default_nls);
				break;
			}
		}
		blobptr += attrsize; /* advance attr  value */
	}

	return 0;
}

static int calc_ntlmv2_hash(struct cifsSesInfo *ses,
			    const struct nls_table *nls_cp)
{
	int rc = 0;
	int len;
	char nt_hash[16];
	struct HMACMD5Context *pctxt;
	wchar_t *user;
	wchar_t *domain;

	pctxt = kmalloc(sizeof(struct HMACMD5Context), GFP_KERNEL);

	if (pctxt == NULL)
		return -ENOMEM;

	/* calculate md4 hash of password */
	E_md4hash(ses->password, nt_hash);

	/* convert Domainname to unicode and uppercase */
	hmac_md5_init_limK_to_64(nt_hash, 16, pctxt);

	/* convert ses->userName to unicode and uppercase */
	len = strlen(ses->userName);
	user = kmalloc(2 + (len * 2), GFP_KERNEL);
	if (user == NULL)
		goto calc_exit_2;
	len = cifs_strtoUCS((__le16 *)user, ses->userName, len, nls_cp);
	UniStrupr(user);
	hmac_md5_update((char *)user, 2*len, pctxt);

	/* convert ses->domainName to unicode and uppercase */
	if (ses->domainName) {
		len = strlen(ses->domainName);

		domain = kmalloc(2 + (len * 2), GFP_KERNEL);
		if (domain == NULL)
			goto calc_exit_1;
		len = cifs_strtoUCS((__le16 *)domain, ses->domainName, len,
					nls_cp);
		/* the following line was removed since it didn't work well
		   with lower cased domain name that passed as an option.
		   Maybe converting the domain name earlier makes sense */
		/* UniStrupr(domain); */

		hmac_md5_update((char *)domain, 2*len, pctxt);

		kfree(domain);
	}
calc_exit_1:
	kfree(user);
calc_exit_2:
	/* BB FIXME what about bytes 24 through 40 of the signing key?
	   compare with the NTLM example */
	hmac_md5_final(ses->ntlmv2_hash, pctxt);

	kfree(pctxt);
	return rc;
}

int
setup_ntlmv2_rsp(struct cifsSesInfo *ses, const struct nls_table *nls_cp)
{
	int rc;
	int baselen;
	struct ntlmv2_resp *buf;
	struct HMACMD5Context context;

	if (ses->server->secType == RawNTLMSSP) {
		if (!ses->domainName) {
			rc = find_domain_name(ses);
			if (rc) {
				cERROR(1, "error %d finding domain name", rc);
				goto setup_ntlmv2_rsp_ret;
			}
		}
	} else {
		rc = build_avpair_blob(ses, nls_cp);
		if (rc) {
			cERROR(1, "error %d building av pair blob", rc);
			return rc;
		}
	}

	baselen = CIFS_SESS_KEY_SIZE + sizeof(struct ntlmv2_resp);
	ses->auth_key.len = baselen + ses->tilen;
	ses->auth_key.response = kmalloc(ses->auth_key.len, GFP_KERNEL);
	if (!ses->auth_key.response) {
		rc = ENOMEM;
		cERROR(1, "%s: Can't allocate auth blob", __func__);
		goto setup_ntlmv2_rsp_ret;
	}

	buf = (struct ntlmv2_resp *)
			(ses->auth_key.response + CIFS_SESS_KEY_SIZE);
	buf->blob_signature = cpu_to_le32(0x00000101);
	buf->reserved = 0;
	buf->time = cpu_to_le64(cifs_UnixTimeToNT(CURRENT_TIME));
	get_random_bytes(&buf->client_chal, sizeof(buf->client_chal));
	buf->reserved2 = 0;

	memcpy(ses->auth_key.response + baselen, ses->tiblob, ses->tilen);

	/* calculate buf->ntlmv2_hash */
	rc = calc_ntlmv2_hash(ses, nls_cp);
	if (rc) {
		cERROR(1, "could not get v2 hash rc %d", rc);
		goto setup_ntlmv2_rsp_ret;
	}
	CalcNTLMv2_response(ses);

	/* now calculate the session key for NTLMv2 */
	hmac_md5_init_limK_to_64(ses->ntlmv2_hash, 16, &context);
	hmac_md5_update(ses->auth_key.response + CIFS_SESS_KEY_SIZE,
				16, &context);
	hmac_md5_final(ses->auth_key.response, &context);

	return 0;

setup_ntlmv2_rsp_ret:
	kfree(ses->tiblob);
	ses->tiblob = NULL;
	ses->tilen = 0;

	return rc;
}

int
calc_seckey(struct cifsSesInfo *ses)
{
	int rc;
	struct crypto_blkcipher *tfm_arc4;
	struct scatterlist sgin, sgout;
	struct blkcipher_desc desc;
	unsigned char sec_key[CIFS_SESS_KEY_SIZE]; /* a nonce */

	get_random_bytes(sec_key, CIFS_SESS_KEY_SIZE);

	tfm_arc4 = crypto_alloc_blkcipher("ecb(arc4)", 0, CRYPTO_ALG_ASYNC);
	if (!tfm_arc4 || IS_ERR(tfm_arc4)) {
		cERROR(1, "could not allocate crypto API arc4\n");
		return PTR_ERR(tfm_arc4);
	}

	desc.tfm = tfm_arc4;

	crypto_blkcipher_setkey(tfm_arc4, ses->auth_key.response,
					CIFS_SESS_KEY_SIZE);

	sg_init_one(&sgin, sec_key, CIFS_SESS_KEY_SIZE);
	sg_init_one(&sgout, ses->ntlmssp.ciphertext, CIFS_CPHTXT_SIZE);

	rc = crypto_blkcipher_encrypt(&desc, &sgout, &sgin, CIFS_CPHTXT_SIZE);
	if (rc) {
		cERROR(1, "could not encrypt session key rc: %d\n", rc);
		crypto_free_blkcipher(tfm_arc4);
		return rc;
	}

	/* make secondary_key/nonce as session key */
	memcpy(ses->auth_key.response, sec_key, CIFS_SESS_KEY_SIZE);
	/* and make len as that of session key only */
	ses->auth_key.len = CIFS_SESS_KEY_SIZE;

	crypto_free_blkcipher(tfm_arc4);

	return 0;
}

void
cifs_crypto_shash_release(struct TCP_Server_Info *server)
{
	if (server->secmech.md5)
		crypto_free_shash(server->secmech.md5);

	if (server->secmech.hmacmd5)
		crypto_free_shash(server->secmech.hmacmd5);

	kfree(server->secmech.sdeschmacmd5);

	kfree(server->secmech.sdescmd5);
}

int
cifs_crypto_shash_allocate(struct TCP_Server_Info *server)
{
	int rc;
	unsigned int size;

	server->secmech.hmacmd5 = crypto_alloc_shash("hmac(md5)", 0, 0);
	if (!server->secmech.hmacmd5 ||
			IS_ERR(server->secmech.hmacmd5)) {
		cERROR(1, "could not allocate crypto hmacmd5\n");
		return PTR_ERR(server->secmech.hmacmd5);
	}

	server->secmech.md5 = crypto_alloc_shash("md5", 0, 0);
	if (!server->secmech.md5 || IS_ERR(server->secmech.md5)) {
		cERROR(1, "could not allocate crypto md5\n");
		rc = PTR_ERR(server->secmech.md5);
		goto crypto_allocate_md5_fail;
	}

	size = sizeof(struct shash_desc) +
			crypto_shash_descsize(server->secmech.hmacmd5);
	server->secmech.sdeschmacmd5 = kmalloc(size, GFP_KERNEL);
	if (!server->secmech.sdeschmacmd5) {
		cERROR(1, "cifs_crypto_shash_allocate: can't alloc hmacmd5\n");
		rc = -ENOMEM;
		goto crypto_allocate_hmacmd5_sdesc_fail;
	}
	server->secmech.sdeschmacmd5->shash.tfm = server->secmech.hmacmd5;
	server->secmech.sdeschmacmd5->shash.flags = 0x0;


	size = sizeof(struct shash_desc) +
			crypto_shash_descsize(server->secmech.md5);
	server->secmech.sdescmd5 = kmalloc(size, GFP_KERNEL);
	if (!server->secmech.sdescmd5) {
		cERROR(1, "cifs_crypto_shash_allocate: can't alloc md5\n");
		rc = -ENOMEM;
		goto crypto_allocate_md5_sdesc_fail;
	}
	server->secmech.sdescmd5->shash.tfm = server->secmech.md5;
	server->secmech.sdescmd5->shash.flags = 0x0;

	return 0;

crypto_allocate_md5_sdesc_fail:
	kfree(server->secmech.sdeschmacmd5);

crypto_allocate_hmacmd5_sdesc_fail:
	crypto_free_shash(server->secmech.md5);

crypto_allocate_md5_fail:
	crypto_free_shash(server->secmech.hmacmd5);

	return rc;
}

void CalcNTLMv2_response(const struct cifsSesInfo *ses)
{
	unsigned int offset = CIFS_SESS_KEY_SIZE + 8;
	struct HMACMD5Context context;

	/* rest of v2 struct already generated */
	memcpy(ses->auth_key.response + offset, ses->cryptKey, 8);
	hmac_md5_init_limK_to_64(ses->ntlmv2_hash, 16, &context);

	hmac_md5_update(ses->auth_key.response + offset,
			ses->auth_key.len - offset, &context);

	hmac_md5_final(ses->auth_key.response + CIFS_SESS_KEY_SIZE, &context);
}
