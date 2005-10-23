/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET command.
 *
 * $Id: set.c 3175 2005-10-23 23:14:41Z jilles $
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set", FALSE, _modinit, _moddeinit,
	"$Id: set.c 3175 2005-10-23 23:14:41Z jilles $",
	"Atheme Development Group <http://www.atheme.org>"
);

static void cs_cmd_set(char *origin);
static void cs_join_entrymsg(chanuser_t *cu);
static void cs_join_url(chanuser_t *cu);
static void cs_keeptopic_newchan(channel_t *c);
static void cs_keeptopic_topicset(channel_t *c);

command_t cs_set = { "SET", "Sets various control flags.",
                        AC_NONE, cs_cmd_set };

list_t *cs_cmdtree;
list_t *cs_helptree;

void _modinit(module_t *m)
{
	cs_cmdtree = module_locate_symbol("chanserv/main", "cs_cmdtree");
	cs_helptree = module_locate_symbol("chanserv/main", "cs_helptree");

        command_add(&cs_set, cs_cmdtree);
	hook_add_event("channel_join");
	hook_add_event("channel_add");
	hook_add_event("topic_set");
	hook_add_hook("channel_join", (void (*)(void *)) cs_join_entrymsg);
	hook_add_hook("channel_join", (void (*)(void *)) cs_join_url);
	hook_add_hook("channel_add", (void (*)(void *)) cs_keeptopic_newchan);
	hook_add_hook("topic_set", (void (*)(void *)) cs_keeptopic_topicset);

	help_addentry(cs_helptree, "SET FOUNDER", "help/cservice/set_founder", NULL);
	help_addentry(cs_helptree, "SET MLOCK", "help/cservice/set_mlock", NULL);
	help_addentry(cs_helptree, "SET SECURE", "help/cservice/set_secure", NULL);
	help_addentry(cs_helptree, "SET VERBOSE", "help/cservice/set_verbose", NULL);
	help_addentry(cs_helptree, "SET URL", "help/cservice/set_url", NULL);
	help_addentry(cs_helptree, "SET EMAIL", "help/cservice/set_email", NULL);
	help_addentry(cs_helptree, "SET ENTRYMSG", "help/cservice/set_entrymsg", NULL);
	help_addentry(cs_helptree, "SET PROPERTY", "help/cservice/set_property", NULL);
	help_addentry(cs_helptree, "SET STAFFONLY", "help/cservice/set_staffonly", NULL);
	help_addentry(cs_helptree, "SET SUCCESSOR", "help/cservice/set_successor", NULL);
	help_addentry(cs_helptree, "SET KEEPTOPIC", "help/cservice/set_keeptopic", NULL);
}

void _moddeinit()
{
	command_delete(&cs_set, cs_cmdtree);
	hook_del_hook("channel_join", (void (*)(void *)) cs_join_entrymsg);
	hook_del_hook("channel_join", (void (*)(void *)) cs_join_url);
	hook_del_hook("channel_add", (void (*)(void *)) cs_keeptopic_newchan);

	help_delentry(cs_helptree, "SET FOUNDER");
	help_delentry(cs_helptree, "SET MLOCK");
	help_delentry(cs_helptree, "SET SECURE");
	help_delentry(cs_helptree, "SET VERBOSE");
	help_delentry(cs_helptree, "SET URL");
	help_delentry(cs_helptree, "SET EMAIL");
	help_delentry(cs_helptree, "SET ENTRYMSG");
	help_delentry(cs_helptree, "SET PROPERTY");
	help_delentry(cs_helptree, "SET STAFFONLY");
	help_delentry(cs_helptree, "SET SUCCESSOR");
	help_delentry(cs_helptree, "SET KEEPTOPIC");
}

struct set_command_ *set_cmd_find(char *origin, char *command);

/* SET <#channel> <setting> <parameters> */
static void cs_cmd_set(char *origin)
{
	char *name = strtok(NULL, " ");
	char *setting = strtok(NULL, " ");
	char *params = strtok(NULL, "");
	struct set_command_ *c;

	if (!name || !setting || !params)
	{
		notice(chansvs.nick, origin, "Insufficient parameters specified for \2SET\2.");
		notice(chansvs.nick, origin, "Syntax: SET <#channel> <setting> <parameters>");
		return;
	}

	/* take the command through the hash table */
	if ((c = set_cmd_find(origin, setting)))
	{
		if (c->func)
			c->func(origin, name, params);
		else
			notice(chansvs.nick, origin, "Invalid setting.  Please use \2HELP\2 for help.");
	}
}

static void cs_set_email(char *origin, char *name, char *params)
{
        user_t *u = user_find(origin);
        mychan_t *mc;
        chanacs_t *ca;
        char *mail = strtok(params, " ");

        if (*name != '#')
        {
                notice(chansvs.nick, origin, "Invalid parameters specified for \2Email\2.");
                return;
        }

        if (!(mc = mychan_find(name)))
        {
                notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
                return;
        }

        if (!(ca = chanacs_find(mc, u->myuser, CA_FLAGS)))
        {
                notice(chansvs.nick, origin, "You are not authorized to execute this command.");
                return;
        }

        if (!mail || !strcasecmp(mail, "NONE") || !strcasecmp(mail, "OFF"))
        {
                if (metadata_find(mc, METADATA_CHANNEL, "email"))
                {
                        metadata_delete(mc, METADATA_CHANNEL, "email");
                        notice(chansvs.nick, origin, "The e-mail address for \2%s\2 was deleted.", mc->name);
                        return;
                }

                notice(chansvs.nick, origin, "The e-mail address for \2%s\2 was not set.", mc->name);
                return;
        }

        if (strlen(mail) >= EMAILLEN)
        {
                notice(chansvs.nick, origin, "Invalid parameters specified for \2EMAIL\2.");
                return;
        }

        if (!validemail(mail))
        {
                notice(chansvs.nick, origin, "\2%s\2 is not a valid e-mail address.", mail);
                return;
        }

        /* we'll overwrite any existing metadata */
        metadata_add(mc, METADATA_CHANNEL, "email", mail);

        snoop("SET:EMAIL \2%s\2 on \2%s\2.", mail, mc->name);

        notice(chansvs.nick, origin, "The e-mail address for \2%s\2 has been set to \2%s\2.", name, mail);
}

static void cs_set_url(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;
	chanacs_t *ca;
	char *url = strtok(params, " ");

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2URL\2.");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if (!(ca = chanacs_find(mc, u->myuser, CA_FLAGS)))
	{
		notice(chansvs.nick, origin, "You are not authorized to execute this command.");
		return;
	}

	/* XXX: I'd like to be able to use /CS SET #channel URL to clear but CS SET won't let me... */
	if (!url || !strcasecmp("OFF", url) || !strcasecmp("NONE", url))
	{
		/* not in a namespace to allow more natural use of SET PROPERTY.
		 * they may be able to introduce spaces, though. c'est la vie.
		 */
		if (metadata_find(mc, METADATA_CHANNEL, "url"))
		{
			metadata_delete(mc, METADATA_CHANNEL, "url");
			notice(chansvs.nick, origin, "The URL for \2%s\2 has been cleared.", name);
			return;
		}

		notice(chansvs.nick, origin, "The URL for \2%s\2 was not set.", name);
		return;
	}

	/* we'll overwrite any existing metadata */
	metadata_add(mc, METADATA_CHANNEL, "url", url);

	snoop("SET:URL \2%s\2 on \2%s\2.", url, mc->name);

	notice(chansvs.nick, origin, "The URL of \2%s\2 has been set to \2%s\2.", name, url);
}

static void cs_set_entrymsg(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;
	chanacs_t *ca;

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if (!(ca = chanacs_find(mc, u->myuser, CA_FLAGS)))
	{
		notice(chansvs.nick, origin, "You are not authorized to execute this command.");
		return;
	}

	/* XXX: I'd like to be able to use /CS SET #channel ENTRYMSG to clear but CS SET won't let me... */
	if (!params || !strcasecmp("OFF", params) || !strcasecmp("NONE", params))
	{
		/* entrymsg is private because users won't see it if they're AKICKED,
		 * if the channel is +i, or if the channel is STAFFONLY
		 */
		if (metadata_find(mc, METADATA_CHANNEL, "private:entrymsg"))
		{
			metadata_delete(mc, METADATA_CHANNEL, "private:entrymsg");
			notice(chansvs.nick, origin, "The entry message for \2%s\2 has been cleared.", name);
			return;
		}

		notice(chansvs.nick, origin, "The entry message for \2%s\2 was not set.", name);
		return;
	}

	/* we'll overwrite any existing metadata */
	metadata_add(mc, METADATA_CHANNEL, "private:entrymsg", params);

	snoop("SET:ENTRYMSG \2%s\2 on \2%s\2.", params, mc->name);

	notice(chansvs.nick, origin, "The entry message for \2%s\2 has been set to \2%s\2", name, params);
}

/*
 * This is how CS SET FOUNDER behaves in the absence of channel passwords:
 *
 * To transfer a channel, the original founder (OF) issues the command:
 *    /CS SET #chan FOUNDER NF
 * where NF is the new founder of the channel.
 *
 * Then, to complete the transfer, the NF must issue the command:
 *    /CS SET #chan FOUNDER NF
 *
 * To cancel the transfer before it completes, the OF can issue the command:
 *    /CS SET #chan FOUNDER OF
 *
 * The purpose of the confirmation step is to prevent users from giving away
 * undesirable channels (e.g. registering #kidsex and transferring to an
 * innocent user.) Originally, we used channel passwords for this purpose.
 */
static void cs_set_founder(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	char *newfounder = strtok(params, " ");
	myuser_t *tmu;
	mychan_t *mc;

	if (!u->myuser)
	{
		notice(chansvs.nick, origin, "You are not logged in.");
		return;
	}

	if (!name || !newfounder)
	{
		notice(chansvs.nick, origin, "Insufficient parameters specified for \2FOUNDER\2.");
		notice(chansvs.nick, origin, "Usage: SET <#channel> FOUNDER <new founder>");
	}

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2FOUNDER\2.");
		return;
	}

	if (!(tmu = myuser_find(newfounder)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", newfounder);
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if (!is_founder(mc, u->myuser))
	{
		/* User is not currently the founder.
		 * Maybe he is trying to complete a transfer?
		 */
		metadata_t *md;

		/* XXX is it portable to compare times like that? */
		if ((u->myuser == tmu)
			&& (md = metadata_find(mc, METADATA_CHANNEL, "private:verify:founderchg:newfounder"))
			&& !irccasecmp(md->value, u->myuser->name)
			&& (md = metadata_find(mc, METADATA_CHANNEL, "private:verify:founderchg:timestamp"))
			&& (atol(md->value) >= u->myuser->registered))
		{
			mychan_t *tmc;
			node_t *n;
			uint32_t i, tcnt;

			/* make sure they're within limits (from cs_cmd_register) */
			for (i = 0, tcnt = 0; i < HASHSIZE; i++)
			{
				LIST_FOREACH(n, mclist[i].head)
				{
					tmc = (mychan_t *)n->data;

					if (is_founder(tmc, tmu))
						tcnt++;
				}
			}

			if ((tcnt >= me.maxchans) && !is_sra(tmu))
			{
				notice(chansvs.nick, origin, "\2%s\2 has too many channels registered.", tmu->name);
				return;
			}

			if (metadata_find(mc, METADATA_CHANNEL, "private:close:closer"))
			{
				notice(chansvs.nick, origin, "\2%s\2 is closed; it cannot be transferred.", mc->name);
				return;
			}

			/* delete all access of self... */
			chanacs_delete(mc, mc->founder, CA_FOUNDER);
			chanacs_delete(mc, tmu, CA_VOP);
			chanacs_delete(mc, tmu, CA_HOP);
			chanacs_delete(mc, tmu, CA_AOP);
			chanacs_delete(mc, tmu, CA_SOP);

			/* add target as founder... */
			mc->founder = tmu;
			chanacs_add(mc, tmu, CA_FOUNDER);

			/* delete transfer metadata */
			metadata_delete(mc, METADATA_CHANNEL, "private:verify:founderchg:newfounder");
			metadata_delete(mc, METADATA_CHANNEL, "private:verify:founderchg:timestamp");

			/* done! */
			snoop("SET:FOUNDER: \2%s\2 -> \2%s\2", mc->name, tmu->name);
			notice(chansvs.nick, origin, "Transfer complete: "
				"\2%s\2 has been set as founder for \2%s\2.", tmu->name, mc->name);

			return;
		}

		notice(chansvs.nick, origin, "You are not the founder of \2%s\2.", mc->name);
		return;
	}

	if (is_founder(mc, tmu))
	{
		/* User is currently the founder and
		 * trying to transfer back to himself.
		 * Maybe he is trying to cancel a transfer?
		 */

		if (metadata_find(mc, METADATA_CHANNEL, "private:verify:founderchg:newfounder"))
		{
			metadata_delete(mc, METADATA_CHANNEL, "private:verify:founderchg:newfounder");
			metadata_delete(mc, METADATA_CHANNEL, "private:verify:founderchg:timestamp");

			notice(chansvs.nick, origin, "The transfer of \2%s\2 has been cancelled.", mc->name);

			return;
		}

		notice(chansvs.nick, origin, "\2%s\2 is already the founder of \2%s\2.", tmu->name, mc->name);
		return;
	}

	/* check for lazy cancellation of outstanding requests */
	if (metadata_find(mc, METADATA_CHANNEL, "private:verify:founderchg:newfounder"))
		notice(chansvs.nick, origin, "The previous transfer request for \2%s\2 has been cancelled.", mc->name);

	metadata_add(mc, METADATA_CHANNEL, "private:verify:founderchg:newfounder", tmu->name);
	metadata_add(mc, METADATA_CHANNEL, "private:verify:founderchg:timestamp", itoa(time(NULL)));

	notice(chansvs.nick, origin, "\2%s\2 can now take ownership of \2%s\2.", tmu->name, mc->name);
	notice(chansvs.nick, origin, "In order to complete the transfer, \2%s\2 must perform the following command:", tmu->name);
	notice(chansvs.nick, origin, "   \2/msg %s SET %s FOUNDER %s\2", chansvs.nick, mc->name, tmu->name);
	notice(chansvs.nick, origin, "After that command is issued, the channel will be transferred.", mc->name);
	notice(chansvs.nick, origin, "To cancel the transfer, use \2/msg %s SET %s FOUNDER %s\2", chansvs.nick, mc->name, mc->founder->name);
}

static void cs_set_mlock(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;
	char modebuf[32], *end, c;
	int add = -1;
	int32_t newlock_on = 0, newlock_off = 0, newlock_limit = 0, flag = 0;
	int32_t mask;
	char *newlock_key = NULL;
	char *letters = strtok(params, " ");
	char *arg;

	if (*name != '#' || !letters)
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2MLOCK\2.");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if (!chanacs_user_has_flag(mc, u, CA_SET))
	{
		notice(chansvs.nick, origin, "You are not authorized to perform this command.");
		return;
	}

	mask = is_ircop(u) ? 0 : ircd->oper_only_modes;

	while (*letters)
	{
		if (*letters != '+' && *letters != '-' && add < 0)
		{
			letters++;
			continue;
		}

		switch ((c = *letters++))
		{
		  case '+':
			  add = 1;
			  break;

		  case '-':
			  add = 0;
			  break;

		  case 'k':
			  if (add)
			  {
				  arg = strtok(NULL, " ");
				  if (!arg)
				  {
					  notice(chansvs.nick, origin, "You need to specify which key to MLOCK.");
					  return;
				  }

				  if (newlock_key)
					  free(newlock_key);

				  newlock_key = sstrdup(arg);
				  newlock_off &= ~CMODE_KEY;
			  }
			  else
			  {
				  if (newlock_key)
				  {
					  free(newlock_key);
					  newlock_key = NULL;
				  }

				  newlock_off |= CMODE_KEY;
			  }

			  break;

		  case 'l':
			  if (add)
			  {
				  arg = strtok(NULL, " ");
				  if(!arg)
				  {
					  notice(chansvs.nick, origin, "You need to specify what limit to MLOCK.");
					  return;
				  }

				  if (atol(arg) <= 0)
				  {
					  notice(chansvs.nick, origin, "You must specify a positive integer for limit.");
					  return;
				  }

				  newlock_limit = atol(arg);
				  newlock_off &= ~CMODE_LIMIT;
			  }
			  else
			  {
				  newlock_limit = 0;
				  newlock_off |= CMODE_LIMIT;
			  }

			  break;

		  default:
			  flag = mode_to_flag(c);

			  if (flag)
			  {
				  if (add)
					  newlock_on |= flag, newlock_off &= ~flag;
				  else
					  newlock_off |= flag, newlock_on &= ~flag;
			  }
		}
	}

	/* save it to mychan */
	/* leave the modes in mask unchanged -- jilles */
	mc->mlock_on = (newlock_on & ~mask) | (mc->mlock_on & mask);
	mc->mlock_off = (newlock_off & ~mask) | (mc->mlock_off & mask);
	mc->mlock_limit = newlock_limit;

	if (mc->mlock_key)
		free(mc->mlock_key);

	mc->mlock_key = newlock_key;

	end = modebuf;
	*end = 0;

	if (mc->mlock_on || mc->mlock_key || mc->mlock_limit)
		end += snprintf(end, sizeof(modebuf) - (end - modebuf), "+%s%s%s", flags_to_string(mc->mlock_on), mc->mlock_key ? "k" : "", mc->mlock_limit ? "l" : "");

	if (mc->mlock_off)
		end += snprintf(end, sizeof(modebuf) - (end - modebuf), "-%s%s%s", flags_to_string(mc->mlock_off), mc->mlock_off & CMODE_KEY ? "k" : "", mc->mlock_off & CMODE_LIMIT ? "l" : "");

	if (*modebuf)
	{
		notice(chansvs.nick, origin, "The MLOCK for \2%s\2 has been set to \2%s\2.", mc->name, modebuf);
		snoop("SET:MLOCK: \2%s\2 to \2%s\2 by \2%s\2", mc->name, modebuf, origin);
	}
	else
	{
		notice(chansvs.nick, origin, "The MLOCK for \2%s\2 has been removed.", mc->name);
		snoop("SET:MLOCK:OFF: \2%s\2 by \2%s\2", mc->name, origin);
	}

	check_modes(mc);

	return;
}

static void cs_set_keeptopic(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2KEEPTOPIC\2");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
	{
		notice(chansvs.nick, origin, "You are not authorized to perform this command.");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MC_KEEPTOPIC & mc->flags)
                {
                        notice(chansvs.nick, origin, "The \2KEEPTOPIC\2 flag is already set for \2%s\2.", mc->name);
                        return;
                }

                snoop("SET:KEEPTOPIC:ON: for \2%s\2 by \2%s\2", mc->name, origin);

                mc->flags |= MC_KEEPTOPIC;

                notice(chansvs.nick, origin, "The \2KEEPTOPIC\2 flag has been set for \2%s\2.", mc->name);

                return;
        }

        else if (!strcasecmp("OFF", params))
        {
                if (!(MC_KEEPTOPIC & mc->flags))
                {
                        notice(chansvs.nick, origin, "The \2KEEPTOPIC\2 flag is not set for \2%s\2.", mc->name);
                        return;
                }

                snoop("SET:KEEPTOPIC:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

                mc->flags &= ~MC_KEEPTOPIC;

                notice(chansvs.nick, origin, "The \2KEEPTOPIC\2 flag has been removed for \2%s\2.", mc->name);

                return;
        }

        else
        {
                notice(chansvs.nick, origin, "Invalid parameters specified for \2KEEPTOPIC\2.");
                return;
        }
}

static void cs_set_secure(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2SECURE\2.");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
	{
		notice(chansvs.nick, origin, "You are not authorized to perform this command.");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MC_SECURE & mc->flags)
		{
			notice(chansvs.nick, origin, "The \2SECURE\2 flag is already set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:SECURE:ON: for \2%s\2 by \2%s\2", mc->name, origin);

		mc->flags |= MC_SECURE;

		notice(chansvs.nick, origin, "The \2SECURE\2 flag has been set for \2%s\2.", mc->name);

		return;
	}

	else if (!strcasecmp("OFF", params))
	{
		if (!(MC_SECURE & mc->flags))
		{
			notice(chansvs.nick, origin, "The \2SECURE\2 flag is not set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:SECURE:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

		mc->flags &= ~MC_SECURE;

		notice(chansvs.nick, origin, "The \2SECURE\2 flag has been removed for \2%s\2.", mc->name);

		return;
	}

	else
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2SECURE\2.");
		return;
	}
}

static void cs_set_successor(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	node_t *n;
	user_t *succ_u;
	myuser_t *mu;
	mychan_t *mc;

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2SUCCESSOR\2.");
		return;
	}

	if (!u->myuser)
	{
		notice(chansvs.nick, origin, "You are not logged in.");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if (!is_founder(mc, u->myuser))
	{
		notice(chansvs.nick, origin, "You are not authorized to perform this operation.");
		return;
	}

	if (!strcasecmp("OFF", params) || !strcasecmp("NONE", params))
	{
		if (!mc->successor)
		{
			notice(chansvs.nick, origin, "There is no successor set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:SUCCESSOR:NONE: \2%s\2", mc->name);

		chanacs_delete(mc, mc->successor, CA_SUCCESSOR);

		notice(chansvs.nick, origin, "\2%s\2 is no longer the successor of \2%s\2.", mc->successor->name, mc->name);
		LIST_FOREACH(n, mc->successor->logins.head)
		{
			succ_u = n->data;
			notice(succ_u->nick, "You (%s) are no longer the successor of \2%s\2.", mc->successor->name, mc->name);
		}

		mc->successor = NULL;

		return;
	}

	if (!(mu = myuser_find(params)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", params);
		return;
	}

	if (mu->flags & MU_NOOP)
	{
		notice(chansvs.nick, origin, "\2%s\2 does not wish to be added to access lists.", mu->name);
		return;
	}

	if (is_successor(mc, mu))
	{
		notice(chansvs.nick, origin, "\2%s\2 is already the successor of \2%s\2.", mu->name, mc->name);
		return;
	}

	chanacs_delete(mc, mu, CA_VOP);
	chanacs_delete(mc, mu, CA_AOP);
	chanacs_delete(mc, mu, CA_SOP);
	mc->successor = mu;
	chanacs_add(mc, mu, CA_SUCCESSOR);

	snoop("SET:SUCCESSOR: \2%s\2 -> \2%s\2", mc->name, mu->name);

	notice(chansvs.nick, origin, "\2%s\2 is now the successor of \2%s\2.", mu->name, mc->name);

	/* if they're online, let them know about it */
	LIST_FOREACH(n, mc->successor->logins.head)
	{
		succ_u = n->data;
		notice(succ_u->nick, "\2%s\2 has set you (%s) as the successor of \2%s\2.", u->myuser->name, mc->successor->name, mc->name);
	}
}

static void cs_set_verbose(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2VERBOSE\2.");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
	{
		notice(chansvs.nick, origin, "You are not authorized to perform this command.");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MC_VERBOSE & mc->flags)
		{
			notice(chansvs.nick, origin, "The \2VERBOSE\2 flag is already set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:VERBOSE:ON: for \2%s\2 by \2%s\2", mc->name, origin);

 		mc->flags |= MC_VERBOSE;

		notice(chansvs.nick, origin, "The \2VERBOSE\2 flag has been set for \2%s\2.", mc->name);

		return;
	}

	else if (!strcasecmp("OFF", params))
	{
		if (!(MC_VERBOSE & mc->flags))
		{
			notice(chansvs.nick, origin, "The \2VERBOSE\2 flag is not set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:VERBOSE:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

		mc->flags &= ~MC_VERBOSE;

		notice(chansvs.nick, origin, "The \2VERBOSE\2 flag has been removed for \2%s\2.", mc->name);

		return;
	}

	else
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2VERBOSE\2.");
		return;
	}
}

static void cs_set_staffonly(char *origin, char *name, char *params)
{
	mychan_t *mc;

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2STAFFONLY\2.");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MC_STAFFONLY & mc->flags)
		{
			notice(chansvs.nick, origin, "The \2STAFFONLY\2 flag is already set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:STAFFONLY:ON: for \2%s\2 by \2%s\2", mc->name, origin);

		mc->flags |= MC_STAFFONLY;

		notice(chansvs.nick, origin, "The \2STAFFONLY\2 flag has been set for \2%s\2.", mc->name);

		return;
	}

	else if (!strcasecmp("OFF", params))
	{
		if (!(MC_STAFFONLY & mc->flags))
		{
			notice(chansvs.nick, origin, "The \2STAFFONLY\2 flag is not set for \2%s\2.", mc->name);
			return;
		}

		snoop("SET:STAFFONLY:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

		mc->flags &= ~MC_STAFFONLY;

		notice(chansvs.nick, origin, "The \2STAFFONLY\2 flag has been removed for \2%s\2.", mc->name);

		return;
	}

	else
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2STAFFONLY\2.");
		return;
	}
}

static void cs_set_property(char *origin, char *name, char *params)
{
	user_t *u = user_find(origin);
	mychan_t *mc;
        char *property = strtok(params, " ");
        char *value = strtok(NULL, "");

	if (*name != '#')
	{
		notice(chansvs.nick, origin, "Invalid parameters specified for \2PROPERTY\2.");
		return;
	}

        if (!property)
        {
                notice(chansvs.nick, origin, "Syntax: SET <#channel> PROPERTY <property> [value]");
                return;
        }

        if (strchr(property, ':') && !is_ircop(u) && !is_sra(u->myuser))
        {
                notice(chansvs.nick, origin, "Invalid property name.");
                return;
        }

	if (!(mc = mychan_find(name)))
	{
		notice(chansvs.nick, origin, "\2%s\2 is not registered.", name);
		return;
	}

	if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
	{
		notice(chansvs.nick, origin, "You are not authorized to perform this command.");
		return;
	}

        snoop("SET:PROPERTY: \2%s\2: \2%s\2/\2%s\2", mc->name, property, value);

	if (mc->metadata.count > me.mdlimit)
	{
		notice(chansvs.nick, origin, "Cannot add \2%s\2 to \2%s\2 metadata table, it is full.",
						property, name);
		return;
	}

	if (!value)
	{
		metadata_t *md = metadata_find(mc, METADATA_CHANNEL, property);

		if (!md)
		{
			notice(chansvs.nick, origin, "Metadata entry \2%s\2 was not set.", property);
			return;
		}

		metadata_delete(mc, METADATA_CHANNEL, property);
		notice(chansvs.nick, origin, "Metadata entry \2%s\2 has been deleted.", property);
		return;
	}

	if (strlen(property) > 32 || strlen(value) > 300)
	{
		notice(chansvs.nick, origin, "Parameters are too long. Aborting.");
		return;
	}

	metadata_add(mc, METADATA_CHANNEL, property, value);
	notice(chansvs.nick, origin, "Metadata entry \2%s\2 added.", property);
}

/* *INDENT-OFF* */

/* commands we understand */
struct set_command_ set_commands[] = {
  { "FOUNDER",    AC_NONE,  cs_set_founder    },
  { "MLOCK",      AC_NONE,  cs_set_mlock      },
  { "SECURE",     AC_NONE,  cs_set_secure     },
  { "SUCCESSOR",  AC_NONE,  cs_set_successor  }, 
  { "VERBOSE",    AC_NONE,  cs_set_verbose    },
  { "URL",	  AC_NONE,  cs_set_url        },
  { "ENTRYMSG",	  AC_NONE,  cs_set_entrymsg   },
  { "PROPERTY",   AC_NONE,  cs_set_property   },
  { "EMAIL",      AC_NONE,  cs_set_email      },
  { "KEEPTOPIC",  AC_NONE, cs_set_keeptopic   },
  { "STAFFONLY",  AC_IRCOP, cs_set_staffonly  },
  { NULL, 0, NULL }
};

/* *INDENT-ON* */

struct set_command_ *set_cmd_find(char *origin, char *command)
{
	user_t *u = user_find(origin);
	struct set_command_ *c;

	for (c = set_commands; c->name; c++)
	{
		if (!strcasecmp(command, c->name))
		{
			/* no special access required, so go ahead... */
			if (c->access == AC_NONE)
				return c;

			/* sra? */
			if ((c->access == AC_SRA) && (is_sra(u->myuser)))
				return c;

			/* ircop? */
			if ((c->access == AC_IRCOP) && (is_ircop(u)))
				return c;

			/* otherwise... */
			else
			{
				notice(chansvs.nick, origin, "You are not authorized to perform this operation.");
				return NULL;
			}
		}
	}

	/* it's a command we don't understand */
	notice(chansvs.nick, origin, "Invalid command. Please use \2HELP\2 for help.");
	return NULL;
}

static void cs_join_entrymsg(chanuser_t *cu)
{
	mychan_t *mc;
	metadata_t *md;

	if (is_internal_client(cu->user))
		return;
	
	if (!(mc = mychan_find(cu->chan->name)))
		return;

	if ((md = metadata_find(mc, METADATA_CHANNEL, "private:entrymsg")))
		notice(chansvs.nick, cu->user->nick, "[%s] %s", mc->name, md->value);
}

static void cs_join_url(chanuser_t *cu)
{
	mychan_t *mc;
	metadata_t *md;

	if (is_internal_client(cu->user))
		return;

	if (!(mc = mychan_find(cu->chan->name)))
		return;

	if ((md = metadata_find(mc, METADATA_CHANNEL, "url")))
		numeric_sts(me.name, 328, cu->user->nick, "%s :%s", mc->name, md->value);
}

/* Called on set of a topic */
static void cs_keeptopic_topicset(channel_t *c)
{
	mychan_t *mc;
	metadata_t *md;
	char *text;

	mc = mychan_find(c->name);

	if (mc == NULL)
		return;

	md = metadata_find(mc, METADATA_CHANNEL, "private:topic:text");
	if (md != NULL)
	{
		if (c->topic != NULL && !strcmp(md->value, c->topic))
			return;
		metadata_delete(mc, METADATA_CHANNEL, "private:topic:text");
	}

	if (metadata_find(mc, METADATA_CHANNEL, "private:topic:setter"))
		metadata_delete(mc, METADATA_CHANNEL, "private:topic:setter");

	if (metadata_find(mc, METADATA_CHANNEL, "private:topic:ts"))
		metadata_delete(mc, METADATA_CHANNEL, "private:topic:ts");

	if (c->topic && c->topic_setter)
	{
		slog(LG_DEBUG, "KeepTopic: topic set for %s by %s: %s", c->name,
			c->topic_setter, c->topic);
		metadata_add(mc, METADATA_CHANNEL, "private:topic:setter",
			c->topic_setter);
		metadata_add(mc, METADATA_CHANNEL, "private:topic:text",
			c->topic);
		metadata_add(mc, METADATA_CHANNEL, "private:topic:ts",
			itoa(c->topicts));
	}
	else
		slog(LG_DEBUG, "KeepTopic: topic cleared for %s", c->name);
}

/* Called on creation of a channel */
static void cs_keeptopic_newchan(channel_t *c)
{
	mychan_t *mc;
	metadata_t *md;
	char *setter;
	char *text;
	time_t channelts;
	time_t topicts;


	if (!(mc = mychan_find(c->name)))
		return;

	if (!(MC_KEEPTOPIC & mc->flags))
		return;

/*
	md = metadata_find(mc, METADATA_CHANNEL, "private:channelts");
	if (md == NULL)
		return;
	channelts = atol(md->value);
	if (channelts == c->ts)
	{
		/* Same channel, let's assume ircd has kept it.
		 * Probably not a good assumption if the ircd doesn't do
		 * topic bursting.
		 * -- jilles */
	/*
		slog(LG_DEBUG, "Not doing keeptopic for %s because of equal channelTS", c->name);
		return;
	}

 */

	md = metadata_find(mc, METADATA_CHANNEL, "private:topic:setter");
	if (md == NULL)
		return;
	setter = md->value;

	md = metadata_find(mc, METADATA_CHANNEL, "private:topic:text");
	if (md == NULL)
		return;
	text = md->value;

	md = metadata_find(mc, METADATA_CHANNEL, "private:topic:ts");
	if (md == NULL)
		return;
	topicts = atol(md->value);

	handle_topic(c, setter, topicts, text);
	topic_sts(c->name, setter, topicts, text);
}
