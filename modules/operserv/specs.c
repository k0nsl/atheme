/*
 * Copyright (c) 2005-2006 Patrick Fish, et al
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains functionality which implements the OService SPECS command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"operserv/specs", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Atheme Development Group <http://www.atheme.org>"
);

static void os_cmd_specs(sourceinfo_t *si, int parc, char *parv[]);

command_t os_specs = { "SPECS", N_("Shows oper flags."), AC_NONE, 2, os_cmd_specs, { .path = "oservice/specs" } };

void _modinit(module_t *m)
{
        service_named_bind_command("operserv", &os_specs);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("operserv", &os_specs);
}

struct
{
	const char *priv;
	const char *nickserv_priv, *chanserv_priv, *general_priv, *operserv_priv;
} privnames[] =
{
	/* NickServ/UserServ */
	{ PRIV_USER_AUSPEX, "view concealed information", NULL, NULL, NULL },
	{ PRIV_USER_ADMIN, "drop accounts, freeze accounts, reset passwords", NULL, NULL, NULL },
	{ PRIV_USER_SENDPASS, "send passwords", NULL, NULL, NULL },
	{ PRIV_USER_VHOST, "set vhosts", NULL, NULL, NULL },
	{ PRIV_USER_FREGISTER, "register accounts on behalf of another user", NULL, NULL, NULL },
	/* ChanServ */
	{ PRIV_CHAN_AUSPEX, NULL, "view concealed information", NULL, NULL },
	{ PRIV_CHAN_ADMIN, NULL, "drop channels, close channels, transfer ownership", NULL, NULL },
	{ PRIV_CHAN_CMODES, NULL, "mlock operator modes", NULL, NULL },
	{ PRIV_JOIN_STAFFONLY, NULL, "join staff channels", NULL, NULL },
	/* NickServ/UserServ+ChanServ */
	{ PRIV_MARK, "mark accounts", "mark channels", NULL, NULL },
	{ PRIV_HOLD, "hold accounts", "hold channels", NULL, NULL },
	{ PRIV_REG_NOLIMIT, NULL, "bypass registration limits", NULL, NULL },
	/* general */
	{ PRIV_SERVER_AUSPEX, NULL, NULL, "view concealed information", NULL },
	{ PRIV_VIEWPRIVS, NULL, NULL, "view privileges of other users", NULL },
	{ PRIV_FLOOD, NULL, NULL, "exempt from flood control", NULL },
	{ PRIV_ADMIN, NULL, NULL, "administer services", NULL },
	{ PRIV_METADATA, NULL, NULL, "edit private metadata", NULL },
	/* OperServ */
	{ PRIV_OMODE, NULL, NULL, NULL, "set channel modes" },
	{ PRIV_AKILL, NULL, NULL, NULL, "add and remove autokills" },
	{ PRIV_MASS_AKILL, NULL, NULL, NULL, "masskill channels or regexes" },
	{ PRIV_JUPE, NULL, NULL, NULL, "jupe servers" },
	{ PRIV_NOOP, NULL, NULL, NULL, "NOOP access" },
	{ PRIV_GLOBAL, NULL, NULL, NULL, "send global notices" },
	{ PRIV_GRANT, NULL, NULL, NULL, "edit oper privileges" },
	{ PRIV_OVERRIDE, NULL, NULL, NULL, "perform actions as any other user" },
	/* -- */
	{ NULL, NULL, NULL, NULL, NULL }
};

static void os_cmd_specs(sourceinfo_t *si, int parc, char *parv[])
{
	user_t *tu = NULL;
	operclass_t *cl = NULL;
	const char *targettype = parv[0];
	const char *target = parv[1];
	char nickserv_privs[BUFSIZE], chanserv_privs[BUFSIZE], general_privs[BUFSIZE], operserv_privs[BUFSIZE];
	int i;

	if (!has_any_privs(si))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to use %s."), si->service->nick);
		return;
	}

	if (targettype != NULL)
	{
		if (!has_priv(si, PRIV_VIEWPRIVS))
		{
			command_fail(si, fault_noprivs, STR_NO_PRIVILEGE, PRIV_VIEWPRIVS);
			return;
		}
		if (target == NULL)
			target = "?";
		if (!strcasecmp(targettype, "USER"))
		{
			tu = user_find_named(target);
			if (tu == NULL)
			{
				command_fail(si, fault_nosuch_target, _("\2%s\2 is not on IRC."), target);
				return;
			}
			if (!has_any_privs_user(tu))
			{
				command_success_nodata(si, _("\2%s\2 is unickserv_privileged."), tu->nick);
				return;
			}
			if (is_internal_client(tu))
			{
				command_fail(si, fault_noprivs, _("\2%s\2 is an internal client."), tu->nick);
				return;
			}
		}
		else if (!strcasecmp(targettype, "OPERCLASS") || !strcasecmp(targettype, "CLASS"))
		{
			cl = operclass_find(target);
			if (cl == NULL)
			{
				command_fail(si, fault_nosuch_target, _("No such oper class \2%s\2."), target);
				return;
			}
		}
		else
		{
			command_fail(si, fault_badparams, _("Valid target types: USER, OPERCLASS."));
			return;
		}
	}
	else
		tu = si->su;

	i = 0;
	*nickserv_privs = *chanserv_privs = *general_privs = *operserv_privs = '\0';
	while (privnames[i].priv != NULL)
	{
		if (targettype == NULL ? has_priv(si, privnames[i].priv) : (tu ? has_priv_user(tu, privnames[i].priv) : has_priv_operclass(cl, privnames[i].priv)))
		{
			if (privnames[i].nickserv_priv != NULL)
			{
				if (*nickserv_privs)
					mowgli_strlcat(nickserv_privs, ", ", sizeof nickserv_privs);
				mowgli_strlcat(nickserv_privs, privnames[i].nickserv_priv, sizeof nickserv_privs);
			}
			if (privnames[i].chanserv_priv != NULL)
			{
				if (*chanserv_privs)
					mowgli_strlcat(chanserv_privs, ", ", sizeof chanserv_privs);
				mowgli_strlcat(chanserv_privs, privnames[i].chanserv_priv, sizeof chanserv_privs);
			}
			if (privnames[i].general_priv != NULL)
			{
				if (*general_privs)
					mowgli_strlcat(general_privs, ", ", sizeof general_privs);
				mowgli_strlcat(general_privs, privnames[i].general_priv, sizeof general_privs);
			}
			if (privnames[i].operserv_priv != NULL)
			{
				if (*operserv_privs)
					mowgli_strlcat(operserv_privs, ", ", sizeof operserv_privs);
				mowgli_strlcat(operserv_privs, privnames[i].operserv_priv, sizeof operserv_privs);
			}
		}
		i++;
	}

	if (targettype == NULL)
		command_success_nodata(si, _("Privileges for \2%s\2:"), get_source_name(si));
	else if (tu)
		command_success_nodata(si, _("Privileges for \2%s\2:"), tu->nick);
	else
		command_success_nodata(si, _("Privileges for oper class \2%s\2:"), cl->name);

	if (*nickserv_privs)
		command_success_nodata(si, _("\2Nicknames/accounts\2: %s"), nickserv_privs);
	if (*chanserv_privs)
		command_success_nodata(si, _("\2Channels\2: %s"), chanserv_privs);
	if (*general_privs)
		command_success_nodata(si, _("\2General\2: %s"), general_privs);
	if (*operserv_privs)
		command_success_nodata(si, _("\2OperServ\2: %s"), operserv_privs);
	command_success_nodata(si, _("End of privileges"));

	if (targettype == NULL)
		logcommand(si, CMDLOG_ADMIN, "SPECS");
	else if (tu)
		logcommand(si, CMDLOG_ADMIN, "SPECS:USER: \2%s!%s@%s\2", tu->nick, tu->user, tu->vhost);
	else
		logcommand(si, CMDLOG_ADMIN, "SPECS:OPERCLASS: \2%s\2", cl->name);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
