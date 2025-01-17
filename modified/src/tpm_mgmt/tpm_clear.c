/*
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2005 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Common Public License as published by
 * IBM Corporation; either version 1 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Common Public License for more details.
 *
 * You should have received a copy of the Common Public License
 * along with this program; if not, a copy can be viewed at
 * http://www.opensource.org/licenses/cpl1.0.php.
 */

#include "tpm_tspi.h"
#include "tpm_utils.h"

//Controled by input options
static BOOL bValue = FALSE;	//If true FORCE CLEAR
static BOOL isWellKnown = FALSE;
TSS_HCONTEXT hContext = 0;
static BOOL askOwnerPass;
static BOOL decodeHexPassword = FALSE;
static BOOL useEnvironment = FALSE;
static const char *ownerpass = NULL;
static const char *ownerpassEnv = NULL;

static inline TSS_RESULT tpmClearOwner(TSS_HTPM a_hTpm, BOOL a_bValue)
{

	TSS_RESULT result = Tspi_TPM_ClearOwner(a_hTpm, a_bValue);
	tspiResult("Tspi_TPM_ClearOwner", result);

	return result;

}

static void help(const char *aCmd)
{
	logCmdHelp(aCmd);
	logUnicodeCmdOption();
	logCmdOption("-o, --owner", _("Set owner secret"));
	logCmdOption("-t, --use-env",
		     _("TPM owner secret is in an environment variable "
			   "whose name is given by -o, respectively"));
	logCmdOption("-x, --use-hex", _("Use hex encoding for owner secret"));
	logCmdOption("-f, --force", _("Use physical presence authorization."));
	logCmdOption("-z, --well-known",
		     _("Use 20 bytes of zeros (TSS_WELL_KNOWN_SECRET) as the TPM secret authorization data"));
}

static int parse(const int aOpt, const char *aArg)
{

	switch (aOpt) {
	case 'o':
		ownerpass = aArg;
		if (!ownerpass)
			askOwnerPass = TRUE;
		else
			askOwnerPass = FALSE;
		isWellKnown = FALSE;
		break;
	case 't':
		useEnvironment = TRUE;
		break;
	case 'x':
		decodeHexPassword = TRUE;
		break;
	case 'f':
		logDebug(_("Changing mode to use force authorization\n"));
		bValue = TRUE;
		break;
	case 'z':
		logDebug(_("Using TSS_WELL_KNOWN_SECRET to authorize the TPM command\n"));
		isWellKnown = TRUE;
		break;
	default:
		return -1;
	}
	return 0;

}

int main(int argc, char **argv)
{

	char *szTpmPasswd = NULL;
	BYTE* pTpmPasswd = NULL;
	int pswd_len;
	TSS_HTPM hTpm;
	TSS_HPOLICY hTpmPolicy;
	int iRc = -1;
	struct option opts[] = {
			{"owner"   , required_argument, NULL, 'o'},
			{"use-env",          no_argument, NULL, 't'},
			{"use-hex",          no_argument, NULL, 'x'},
			{"force", no_argument, NULL, 'f'},
			{"well-known", no_argument, NULL, 'z'},
	};
	BYTE well_known[] = TSS_WELL_KNOWN_SECRET;

        initIntlSys();

	if (genericOptHandler
	    (argc, argv, "o:txfz", opts, sizeof(opts) / sizeof(struct option),
	     parse, help) != 0)
		goto out;

	if (contextCreate(&hContext) != TSS_SUCCESS)
		goto out;

	if (contextConnect(hContext) != TSS_SUCCESS)
		goto out_close;

	if (contextGetTpm(hContext, &hTpm) != TSS_SUCCESS)
		goto out_close;

	if (!bValue) {
		if (isWellKnown){
			szTpmPasswd = (char *)well_known;
			pswd_len = sizeof(well_known);
		}else{
			if (askOwnerPass) {

				szTpmPasswd = GETPASSWD(_("Enter owner password: "), &pswd_len, FALSE);
				if (!szTpmPasswd) {
					logMsg(_("Failed to get password\n"));
					goto out_close;
				}
				ownerpass = szTpmPasswd;
			}
				
			if (ownerpass && !askOwnerPass && useEnvironment) {
				ownerpassEnv = ownerpass;
				ownerpass = getenv(ownerpassEnv);
				if (!ownerpass) {
					logMsg(_("%s is not defined\n"), ownerpassEnv);
					goto out_close;
				}
			}
			
		}

		if (policyGet(hTpm, &hTpmPolicy) != TSS_SUCCESS)
			goto out_close;

		if( isWellKnown ) {
			if (policySetSecret(hTpmPolicy, pswd_len,
						(BYTE *)szTpmPasswd) != TSS_SUCCESS)
				goto out_close;
		}
		else if( decodeHexPassword ) {
				if( hex2bytea(ownerpass, &pTpmPasswd, &pswd_len) != 0 ) {
					logMsg(_("Invalid hex TPM owner secret\n"));
					goto out_close;
				}
				if( Tspi_Policy_SetSecret(hTpmPolicy, TSS_SECRET_MODE_PLAIN, pswd_len,
							pTpmPasswd) != TSS_SUCCESS)
					goto out_close;
		}
		else {
			if( pswd_len < 0 )
				pswd_len = strlen(ownerpass);
			if (policySetSecret(hTpmPolicy, pswd_len, (BYTE *)ownerpass) != TSS_SUCCESS)
				goto out_close;
		}
	}
	//Setup complete attempt command
	if (tpmClearOwner(hTpm, bValue) != TSS_SUCCESS)
		goto out_close;

	//Command successful
	iRc = 0;
	logSuccess(argv[0]);

	logMsg( _("TPM Successfuly Cleared.  You need to reboot to complete this operation.  After reboot the TPM will be in the default state: unowned, disabled and inactive.\n") );

	//Cleanup
      out_close:
	if (szTpmPasswd && !isWellKnown)
		shredPasswd(szTpmPasswd);
	if( pTpmPasswd )
		shredByteArray(pTpmPasswd, pswd_len);

	contextClose(hContext);

      out:
	return iRc;
}
