/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2005 - 2014 Red Hat, Inc.
 * Copyright (C) 2006 - 2008 Novell, Inc.
 */

#include "nm-default.h"

#include "nm-ip4-config.h"

#include <string.h>
#include <arpa/inet.h>

#include "nm-utils/nm-dedup-multi.h"

#include "nm-utils.h"
#include "platform/nmp-object.h"
#include "platform/nm-platform.h"
#include "platform/nm-platform-utils.h"
#include "NetworkManagerUtils.h"
#include "nm-core-internal.h"

#include "introspection/org.freedesktop.NetworkManager.IP4Config.h"

/*****************************************************************************/

/* internal guint32 are assigned to gobject properties of type uint. Ensure, that uint is large enough */
G_STATIC_ASSERT (sizeof (uint) >= sizeof (guint32));
G_STATIC_ASSERT (G_MAXUINT >= 0xFFFFFFFF);

/*****************************************************************************/

static gboolean
_route_valid (const NMPlatformIP4Route *r)
{
	return    r
	       && r->plen <= 32
	       && r->network == nm_utils_ip4_address_clear_host_address (r->network, r->plen);
}

/*****************************************************************************/

gboolean
nm_ip_config_obj_id_equal_ip4_address (const NMPlatformIP4Address *a,
                                       const NMPlatformIP4Address *b)
{
	return    a->address == b->address
	       && a->plen == b->plen
	       && ((a->peer_address ^ b->peer_address) & _nm_utils_ip4_prefix_to_netmask (a->plen)) == 0;
}

gboolean
nm_ip_config_obj_id_equal_ip6_address (const NMPlatformIP6Address *a,
                                       const NMPlatformIP6Address *b)
{
	return IN6_ARE_ADDR_EQUAL (&a->address, &b->address);
}

gboolean
nm_ip_config_obj_id_equal_ip4_route (const NMPlatformIP4Route *r_a,
                                     const NMPlatformIP4Route *r_b)
{
	return nm_platform_ip4_route_cmp (r_a, r_b, NM_PLATFORM_IP_ROUTE_CMP_TYPE_DST) == 0;
}

gboolean
nm_ip_config_obj_id_equal_ip6_route (const NMPlatformIP6Route *r_a,
                                     const NMPlatformIP6Route *r_b)
{
	return nm_platform_ip6_route_cmp (r_a, r_b, NM_PLATFORM_IP_ROUTE_CMP_TYPE_DST) == 0;
}

static guint
_idx_obj_id_hash (const NMDedupMultiIdxType *idx_type,
                  const NMDedupMultiObj *obj)
{
	const NMPObject *o = (NMPObject *) obj;
	guint h;

	switch (NMP_OBJECT_GET_TYPE (o)) {
	case NMP_OBJECT_TYPE_IP4_ADDRESS:
		h = 1550630563;
		h = NM_HASH_COMBINE (h, o->ip4_address.address);
		h = NM_HASH_COMBINE (h, o->ip_address.plen);
		h = NM_HASH_COMBINE (h, nm_utils_ip4_address_clear_host_address (o->ip4_address.peer_address, o->ip_address.plen));
		break;
	case NMP_OBJECT_TYPE_IP6_ADDRESS:
		h = 851146661;
		h = NM_HASH_COMBINE_IN6ADDR (h, &o->ip6_address.address);
		break;
	case NMP_OBJECT_TYPE_IP4_ROUTE:
		h = 40303327;
		h = NM_HASH_COMBINE (h, nm_platform_ip4_route_hash (NMP_OBJECT_CAST_IP4_ROUTE (o), NM_PLATFORM_IP_ROUTE_CMP_TYPE_DST));
		break;
	case NMP_OBJECT_TYPE_IP6_ROUTE:
		h = 577629323;
		h = NM_HASH_COMBINE (h, nm_platform_ip6_route_hash (NMP_OBJECT_CAST_IP6_ROUTE (o), NM_PLATFORM_IP_ROUTE_CMP_TYPE_DST));
		break;
	default:
		g_return_val_if_reached (0);
	};

	return h;
}

static gboolean
_idx_obj_id_equal (const NMDedupMultiIdxType *idx_type,
                   const NMDedupMultiObj *obj_a,
                   const NMDedupMultiObj *obj_b)
{
	const NMPObject *o_a = (NMPObject *) obj_a;
	const NMPObject *o_b = (NMPObject *) obj_b;

	nm_assert (NMP_OBJECT_GET_TYPE (o_a) == NMP_OBJECT_GET_TYPE (o_b));

	switch (NMP_OBJECT_GET_TYPE (o_a)) {
	case NMP_OBJECT_TYPE_IP4_ADDRESS:
		return nm_ip_config_obj_id_equal_ip4_address (NMP_OBJECT_CAST_IP4_ADDRESS (o_a), NMP_OBJECT_CAST_IP4_ADDRESS (o_b));
	case NMP_OBJECT_TYPE_IP6_ADDRESS:
		return nm_ip_config_obj_id_equal_ip6_address (NMP_OBJECT_CAST_IP6_ADDRESS (o_a), NMP_OBJECT_CAST_IP6_ADDRESS (o_b));
	case NMP_OBJECT_TYPE_IP4_ROUTE:
		return nm_ip_config_obj_id_equal_ip4_route (&o_a->ip4_route, &o_b->ip4_route);
	case NMP_OBJECT_TYPE_IP6_ROUTE:
		return nm_ip_config_obj_id_equal_ip6_route (&o_a->ip6_route, &o_b->ip6_route);
	default:
		g_return_val_if_reached (FALSE);
	};
}

static const NMDedupMultiIdxTypeClass _dedup_multi_idx_type_class = {
	.idx_obj_id_hash = _idx_obj_id_hash,
	.idx_obj_id_equal = _idx_obj_id_equal,
};

void
nm_ip_config_dedup_multi_idx_type_init (NMIPConfigDedupMultiIdxType *idx_type,
                                        NMPObjectType obj_type)
{
	nm_dedup_multi_idx_type_init ((NMDedupMultiIdxType *) idx_type,
	                              &_dedup_multi_idx_type_class);
	idx_type->obj_type = obj_type;
}

/*****************************************************************************/

gboolean
_nm_ip_config_add_obj (NMDedupMultiIndex *multi_idx,
                       NMIPConfigDedupMultiIdxType *idx_type,
                       int ifindex,
                       const NMPObject *obj_new,
                       const NMPlatformObject *pl_new,
                       gboolean merge,
                       gboolean append_force,
                       const NMPObject **out_obj_new)
{
	NMPObject obj_new_stackinit;
	const NMDedupMultiEntry *entry_old;
	const NMDedupMultiEntry *entry_new;

	nm_assert (multi_idx);
	nm_assert (idx_type);
	nm_assert (NM_IN_SET (idx_type->obj_type, NMP_OBJECT_TYPE_IP4_ADDRESS,
	                                          NMP_OBJECT_TYPE_IP4_ROUTE,
	                                          NMP_OBJECT_TYPE_IP6_ADDRESS,
	                                          NMP_OBJECT_TYPE_IP6_ROUTE));
	nm_assert (ifindex > 0);

	/* we go through extra lengths to accept a full obj_new object. That one,
	 * can be reused by increasing the ref-count. */
	if (!obj_new) {
		nm_assert (pl_new);
		obj_new = nmp_object_stackinit (&obj_new_stackinit, idx_type->obj_type, pl_new);
		obj_new_stackinit.object.ifindex = ifindex;
	} else {
		nm_assert (!pl_new);
		nm_assert (NMP_OBJECT_GET_TYPE (obj_new) == idx_type->obj_type);
		if (obj_new->object.ifindex != ifindex) {
			obj_new = nmp_object_stackinit_obj (&obj_new_stackinit, obj_new);
			obj_new_stackinit.object.ifindex = ifindex;
		}
	}
	nm_assert (NMP_OBJECT_GET_TYPE (obj_new) == idx_type->obj_type);
	nm_assert (nmp_object_is_alive (obj_new));

	entry_old = nm_dedup_multi_index_lookup_obj (multi_idx, &idx_type->parent, obj_new);

	if (entry_old) {
		gboolean modified = FALSE;
		const NMPObject *obj_old = entry_old->obj;

		if (nmp_object_equal (obj_new, obj_old)) {
			nm_dedup_multi_entry_set_dirty (entry_old, FALSE);
			goto append_force_and_out;
		}

		/* if @merge, we merge the new object with the existing one.
		 * Otherwise, we replace it entirely. */
		if (merge) {
			switch (idx_type->obj_type) {
			case NMP_OBJECT_TYPE_IP4_ADDRESS:
			case NMP_OBJECT_TYPE_IP6_ADDRESS:
				/* we want to keep the maximum addr_source. But since we expect
				 * that usually we already add the maxiumum right away, we first try to
				 * add the new address (replacing the old one). Only if we later
				 * find out that addr_source is now lower, we fix it.
				 */
				if (obj_new->ip_address.addr_source < obj_old->ip_address.addr_source) {
					obj_new = nmp_object_stackinit_obj (&obj_new_stackinit, obj_new);
					obj_new_stackinit.ip_address.addr_source = obj_old->ip_address.addr_source;
					modified = TRUE;
				}

				/* for addresses that we read from the kernel, we keep the timestamps as defined
				 * by the previous source (item_old). The reason is, that the other source configured the lifetimes
				 * with "what should be" and the kernel values are "what turned out after configuring it".
				 *
				 * For other sources, the longer lifetime wins. */
				if (   (   obj_new->ip_address.addr_source == NM_IP_CONFIG_SOURCE_KERNEL
				        && obj_old->ip_address.addr_source != NM_IP_CONFIG_SOURCE_KERNEL)
				    || nm_platform_ip_address_cmp_expiry (NMP_OBJECT_CAST_IP_ADDRESS (obj_old), NMP_OBJECT_CAST_IP_ADDRESS(obj_new)) > 0) {
					obj_new = nmp_object_stackinit_obj (&obj_new_stackinit, obj_new);
					obj_new_stackinit.ip_address.timestamp = NMP_OBJECT_CAST_IP_ADDRESS (obj_old)->timestamp;
					obj_new_stackinit.ip_address.lifetime  = NMP_OBJECT_CAST_IP_ADDRESS (obj_old)->lifetime;
					obj_new_stackinit.ip_address.preferred = NMP_OBJECT_CAST_IP_ADDRESS (obj_old)->preferred;
					modified = TRUE;
				}
				break;
			case NMP_OBJECT_TYPE_IP4_ROUTE:
			case NMP_OBJECT_TYPE_IP6_ROUTE:
				/* we want to keep the maximum rt_source. But since we expect
				 * that usually we already add the maxiumum right away, we first try to
				 * add the new route (replacing the old one). Only if we later
				 * find out that rt_source is now lower, we fix it.
				 */
				if (obj_new->ip_route.rt_source < obj_old->ip_route.rt_source) {
					obj_new = nmp_object_stackinit_obj (&obj_new_stackinit, obj_new);
					obj_new_stackinit.ip_route.rt_source = obj_old->ip_route.rt_source;
					modified = TRUE;
				}
				break;
			default:
				nm_assert_not_reached ();
				break;
			}

			if (   modified
			    && nmp_object_equal (obj_new, obj_old)) {
				nm_dedup_multi_entry_set_dirty (entry_old, FALSE);
				goto append_force_and_out;
			}
		}
	}

	if (!nm_dedup_multi_index_add_full (multi_idx,
	                                    &idx_type->parent,
	                                    obj_new,
	                                    NM_DEDUP_MULTI_IDX_MODE_APPEND,
	                                    NULL,
	                                    entry_old ?: NM_DEDUP_MULTI_ENTRY_MISSING,
	                                    NULL,
	                                    &entry_new,
	                                    NULL)) {
		nm_assert_not_reached ();
		NM_SET_OUT (out_obj_new, NULL);
		return FALSE;
	}

	NM_SET_OUT (out_obj_new, entry_new->obj);
	return TRUE;

append_force_and_out:
	NM_SET_OUT (out_obj_new, entry_old->obj);
	if (append_force) {
		if (nm_dedup_multi_entry_reorder (entry_old, NULL, TRUE))
			return TRUE;
	}
	return FALSE;
}

/**
 * _nm_ip_config_lookup_ip_route:
 * @multi_idx:
 * @idx_type:
 * @needle:
 * @cmp_type: after lookup, filter the result by comparing with @cmp_type. Only
 *   return the result, if it compares equal to @needle according to this @cmp_type.
 *   Note that the index uses %NM_PLATFORM_IP_ROUTE_CMP_TYPE_DST type, so passing
 *   that compare-type means not to filter any further.
 *
 * Returns: the found entry or %NULL.
 */
const NMDedupMultiEntry *
_nm_ip_config_lookup_ip_route (const NMDedupMultiIndex *multi_idx,
                               const NMIPConfigDedupMultiIdxType *idx_type,
                               const NMPObject *needle,
                               NMPlatformIPRouteCmpType cmp_type)
{
	const NMDedupMultiEntry *entry;

	nm_assert (multi_idx);
	nm_assert (idx_type);
	nm_assert (NM_IN_SET (idx_type->obj_type, NMP_OBJECT_TYPE_IP4_ROUTE, NMP_OBJECT_TYPE_IP6_ROUTE));
	nm_assert (NMP_OBJECT_GET_TYPE (needle) == idx_type->obj_type);

	entry = nm_dedup_multi_index_lookup_obj (multi_idx,
	                                         &idx_type->parent,
	                                         needle);
	if (!entry)
		return NULL;

	if (cmp_type == NM_PLATFORM_IP_ROUTE_CMP_TYPE_DST)
		nm_assert (nm_platform_ip4_route_cmp (NMP_OBJECT_CAST_IP4_ROUTE (entry->obj), NMP_OBJECT_CAST_IP4_ROUTE (needle), cmp_type) == 0);
	else {
		if (nm_platform_ip4_route_cmp (NMP_OBJECT_CAST_IP4_ROUTE (entry->obj),
		                               NMP_OBJECT_CAST_IP4_ROUTE (needle),
		                               cmp_type) != 0)
			return NULL;
	}
	return entry;
}

/*****************************************************************************/

NM_GOBJECT_PROPERTIES_DEFINE (NMIP4Config,
	PROP_MULTI_IDX,
	PROP_IFINDEX,
	PROP_ADDRESS_DATA,
	PROP_ADDRESSES,
	PROP_ROUTE_DATA,
	PROP_ROUTES,
	PROP_GATEWAY,
	PROP_NAMESERVERS,
	PROP_DOMAINS,
	PROP_SEARCHES,
	PROP_DNS_OPTIONS,
	PROP_WINS_SERVERS,
	PROP_DNS_PRIORITY,
);

typedef struct {
	bool never_default:1;
	bool metered:1;
	bool has_gateway:1;
	guint32 gateway;
	guint32 mss;
	guint32 mtu;
	int ifindex;
	NMIPConfigSource mtu_source;
	gint dns_priority;
	gint64 route_metric;
	GArray *nameservers;
	GPtrArray *domains;
	GPtrArray *searches;
	GPtrArray *dns_options;
	GArray *nis;
	char *nis_domain;
	GArray *wins;
	GVariant *address_data_variant;
	GVariant *addresses_variant;
	GVariant *route_data_variant;
	GVariant *routes_variant;
	NMDedupMultiIndex *multi_idx;
	union {
		NMIPConfigDedupMultiIdxType idx_ip4_addresses_;
		NMDedupMultiIdxType idx_ip4_addresses;
	};
	union {
		NMIPConfigDedupMultiIdxType idx_ip4_routes_;
		NMDedupMultiIdxType idx_ip4_routes;
	};
} NMIP4ConfigPrivate;

struct _NMIP4Config {
	NMExportedObject parent;
	NMIP4ConfigPrivate _priv;
};

struct _NMIP4ConfigClass {
	NMExportedObjectClass parent;
};

G_DEFINE_TYPE (NMIP4Config, nm_ip4_config, NM_TYPE_EXPORTED_OBJECT)

#define NM_IP4_CONFIG_GET_PRIVATE(self) _NM_GET_PRIVATE(self, NMIP4Config, NM_IS_IP4_CONFIG)

/*****************************************************************************/

static void _add_address (NMIP4Config *self, const NMPObject *obj_new, const NMPlatformIP4Address *new);
static void _add_route (NMIP4Config *self, const NMPObject *obj_new, const NMPlatformIP4Route *new);
static const NMDedupMultiEntry *_lookup_route (const NMIP4Config *self,
                                               const NMPObject *needle,
                                               NMPlatformIPRouteCmpType cmp_type);

/*****************************************************************************/

int
nm_ip4_config_get_ifindex (const NMIP4Config *self)
{
	return NM_IP4_CONFIG_GET_PRIVATE (self)->ifindex;
}

NMDedupMultiIndex *
nm_ip4_config_get_multi_idx (const NMIP4Config *self)
{
	return NM_IP4_CONFIG_GET_PRIVATE (self)->multi_idx;
}

/*****************************************************************************/

static gboolean
_ipv4_is_zeronet (in_addr_t network)
{
	/* Same as ipv4_is_zeronet() from kernel's include/linux/in.h. */
	return (network & htonl(0xff000000)) == htonl(0x00000000);
}

/*****************************************************************************/

const NMDedupMultiHeadEntry *
nm_ip4_config_lookup_addresses (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return nm_dedup_multi_index_lookup_head (priv->multi_idx,
	                                         &priv->idx_ip4_addresses,
	                                         NULL);
}

void
nm_ip_config_iter_ip4_address_init (NMDedupMultiIter *ipconf_iter, const NMIP4Config *self)
{
	g_return_if_fail (NM_IS_IP4_CONFIG (self));
	nm_dedup_multi_iter_init (ipconf_iter, nm_ip4_config_lookup_addresses (self));
}

/*****************************************************************************/

const NMDedupMultiHeadEntry *
nm_ip4_config_lookup_routes (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return nm_dedup_multi_index_lookup_head (priv->multi_idx,
	                                         &priv->idx_ip4_routes,
	                                         NULL);
}

void
nm_ip_config_iter_ip4_route_init (NMDedupMultiIter *ipconf_iter, const NMIP4Config *self)
{
	g_return_if_fail (NM_IS_IP4_CONFIG (self));
	nm_dedup_multi_iter_init (ipconf_iter, nm_ip4_config_lookup_routes (self));
}

/*****************************************************************************/

static void
_notify_addresses (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	nm_clear_g_variant (&priv->address_data_variant);
	nm_clear_g_variant (&priv->addresses_variant);
	_notify (self, PROP_ADDRESS_DATA);
	_notify (self, PROP_ADDRESSES);
}

static void
_notify_routes (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	nm_clear_g_variant (&priv->route_data_variant);
	nm_clear_g_variant (&priv->routes_variant);
	_notify (self, PROP_ROUTE_DATA);
	_notify (self, PROP_ROUTES);
}

/*****************************************************************************/

/**
 * nm_ip4_config_capture_resolv_conf():
 * @nameservers: array of guint32
 * @rc_contents: the contents of a resolv.conf or %NULL to read /etc/resolv.conf
 *
 * Reads all resolv.conf IPv4 nameservers and adds them to @nameservers.
 *
 * Returns: %TRUE if nameservers were added, %FALSE if @nameservers is unchanged
 */
gboolean
nm_ip4_config_capture_resolv_conf (GArray *nameservers,
                                   GPtrArray *dns_options,
                                   const char *rc_contents)
{
	GPtrArray *read_ns, *read_options;
	guint i, j;
	gboolean changed = FALSE;

	g_return_val_if_fail (nameservers != NULL, FALSE);

	read_ns = nm_utils_read_resolv_conf_nameservers (rc_contents);
	if (!read_ns)
		return FALSE;

	for (i = 0; i < read_ns->len; i++) {
		const char *s = g_ptr_array_index (read_ns, i);
		guint32 ns = 0;

		if (!inet_pton (AF_INET, s, (void *) &ns) || !ns)
			continue;

		/* Ignore duplicates */
		for (j = 0; j < nameservers->len; j++) {
			if (g_array_index (nameservers, guint32, j) == ns)
				break;
		}

		if (j == nameservers->len) {
			g_array_append_val (nameservers, ns);
			changed = TRUE;
		}
	}
	g_ptr_array_unref (read_ns);

	if (dns_options) {
		read_options = nm_utils_read_resolv_conf_dns_options (rc_contents);
		if (!read_options)
			return changed;

		for (i = 0; i < read_options->len; i++) {
			const char *s = g_ptr_array_index (read_options, i);

			if (_nm_utils_dns_option_validate (s, NULL, NULL, FALSE, _nm_utils_dns_option_descs) &&
				_nm_utils_dns_option_find_idx (dns_options, s) < 0) {
				g_ptr_array_add (dns_options, g_strdup (s));
				changed = TRUE;
			}
		}
		g_ptr_array_unref (read_options);
	}

	return changed;
}

/*****************************************************************************/

static gint
_addresses_sort_cmp_get_prio (in_addr_t addr)
{
	if (nm_utils_ip4_address_is_link_local (addr))
		return 0;
	return 1;
}

static int
_addresses_sort_cmp (gconstpointer a, gconstpointer b, gpointer user_data)
{
	gint p1, p2;
	const NMPlatformIP4Address *a1 = NMP_OBJECT_CAST_IP4_ADDRESS (*((const NMPObject **) a));
	const NMPlatformIP4Address *a2 = NMP_OBJECT_CAST_IP4_ADDRESS (*((const NMPObject **) b));
	guint32 n1, n2;

	/* Sort by address type. For example link local will
	 * be sorted *after* a global address. */
	p1 = _addresses_sort_cmp_get_prio (a1->address);
	p2 = _addresses_sort_cmp_get_prio (a2->address);
	if (p1 != p2)
		return p1 > p2 ? -1 : 1;

	/* Sort the addresses based on their source. */
	if (a1->addr_source != a2->addr_source)
		return a1->addr_source > a2->addr_source ? -1 : 1;

	if ((a1->label[0] == '\0') != (a2->label[0] == '\0'))
		return (a1->label[0] == '\0') ? -1 : 1;

	/* Finally, sort addresses lexically. We compare only the
	 * network part so that the order of addresses in the same
	 * subnet (and thus also the primary/secondary role) is
	 * preserved.
	 */
	n1 = a1->address & _nm_utils_ip4_prefix_to_netmask (a1->plen);
	n2 = a2->address & _nm_utils_ip4_prefix_to_netmask (a2->plen);

	return memcmp (&n1, &n2, sizeof (guint32));
}

/*****************************************************************************/

static int
sort_captured_addresses (const CList *lst_a, const CList *lst_b, gconstpointer user_data)
{
	const NMPlatformIP4Address *addr_a = NMP_OBJECT_CAST_IP4_ADDRESS (c_list_entry (lst_a, NMDedupMultiEntry, lst_entries)->obj);
	const NMPlatformIP4Address *addr_b = NMP_OBJECT_CAST_IP4_ADDRESS (c_list_entry (lst_b, NMDedupMultiEntry, lst_entries)->obj);

	/* Primary addresses first */
	return NM_FLAGS_HAS (addr_a->n_ifa_flags, IFA_F_SECONDARY) -
	       NM_FLAGS_HAS (addr_b->n_ifa_flags, IFA_F_SECONDARY);
}

NMIP4Config *
nm_ip4_config_capture (NMDedupMultiIndex *multi_idx, NMPlatform *platform, int ifindex, gboolean capture_resolv_conf)
{
	NMIP4Config *self;
	NMIP4ConfigPrivate *priv;
	guint32 lowest_metric;
	guint32 old_gateway = 0;
	gboolean old_has_gateway = FALSE;
	const NMDedupMultiHeadEntry *head_entry;
	NMDedupMultiIter iter;
	const NMPObject *plobj = NULL;
	gboolean has_addresses = FALSE;

	nm_assert (ifindex > 0);

	/* Slaves have no IP configuration */
	if (nm_platform_link_get_master (platform, ifindex) > 0)
		return NULL;

	self = nm_ip4_config_new (multi_idx, ifindex);
	priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	head_entry = nm_platform_lookup_addrroute (platform,
	                                           NMP_OBJECT_TYPE_IP4_ADDRESS,
	                                           ifindex);
	if (head_entry) {
		nmp_cache_iter_for_each (&iter, head_entry, &plobj) {
			if (!_nm_ip_config_add_obj (priv->multi_idx,
			                            &priv->idx_ip4_addresses_,
			                            ifindex,
			                            plobj,
			                            NULL,
			                            FALSE,
			                            TRUE,
			                            NULL))
				nm_assert_not_reached ();
		}
		head_entry = nm_ip4_config_lookup_addresses (self);
		nm_assert (head_entry);
		nm_dedup_multi_head_entry_sort (head_entry,
		                                sort_captured_addresses,
		                                NULL);
		has_addresses = TRUE;
	}

	head_entry = nm_platform_lookup_addrroute (platform,
	                                           NMP_OBJECT_TYPE_IP4_ROUTE,
	                                           ifindex);

	/* Extract gateway from default route */
	old_gateway = priv->gateway;
	old_has_gateway = priv->has_gateway;

	lowest_metric = G_MAXUINT32;
	priv->has_gateway = FALSE;
	nmp_cache_iter_for_each (&iter, head_entry, &plobj) {
		const NMPlatformIP4Route *route = NMP_OBJECT_CAST_IP4_ROUTE (plobj);

		if (   !route->table_coerced
		    && NM_PLATFORM_IP_ROUTE_IS_DEFAULT (route)
		    && route->rt_source != NM_IP_CONFIG_SOURCE_RTPROT_KERNEL) {
			if (route->metric < lowest_metric) {
				priv->gateway = route->gateway;
				lowest_metric = route->metric;
			}
			priv->has_gateway = TRUE;
		}

		if (route->table_coerced)
			continue;
		if (route->rt_source == NM_IP_CONFIG_SOURCE_RTPROT_KERNEL)
			continue;
		if (NM_PLATFORM_IP_ROUTE_IS_DEFAULT (route))
			continue;
		_add_route (self, plobj, NULL);
	}

	/* we detect the route metric based on the default route. All non-default
	 * routes have their route metrics explicitly set. */
	priv->route_metric = priv->has_gateway ? (gint64) lowest_metric : (gint64) -1;

	/* If the interface has the default route, and has IPv4 addresses, capture
	 * nameservers from /etc/resolv.conf.
	 */
	if (has_addresses && priv->has_gateway && capture_resolv_conf) {
		if (nm_ip4_config_capture_resolv_conf (priv->nameservers, priv->dns_options, NULL))
			_notify (self, PROP_NAMESERVERS);
	}

	/* actually, nobody should be connected to the signal, just to be sure, notify */
	_notify_addresses (self);
	_notify_routes (self);
	if (   priv->gateway != old_gateway
	    || priv->has_gateway != old_has_gateway)
		_notify (self, PROP_GATEWAY);

	return self;
}

gboolean
nm_ip4_config_commit (const NMIP4Config *self,
                      NMPlatform *platform,
                      guint32 default_route_metric)
{
	const NMIP4ConfigPrivate *priv;
	gs_unref_ptrarray GPtrArray *addresses = NULL;
	gs_unref_ptrarray GPtrArray *routes = NULL;
	gs_unref_ptrarray GPtrArray *ip4_dev_route_blacklist = NULL;
	int ifindex;
	guint i;
	gboolean success = TRUE;

	g_return_val_if_fail (NM_IS_IP4_CONFIG (self), FALSE);

	priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	ifindex = nm_ip4_config_get_ifindex (self);
	g_return_val_if_fail (ifindex > 0, FALSE);

	addresses = nm_dedup_multi_objs_to_ptr_array_head (nm_ip4_config_lookup_addresses (self),
	                                                   NULL, NULL);

	routes = nm_dedup_multi_objs_to_ptr_array_head (nm_ip4_config_lookup_routes (self),
	                                                NULL, NULL);

	if (addresses) {
		/* For IPv6, we explicitly add the device-routes (onlink) to NMIP6Config.
		 * As we don't do that for IPv4, add it here shortly before syncing
		 * the routes. */
		for (i = 0; i < addresses->len; i++) {
			const NMPObject *o = addresses->pdata[i];
			const NMPlatformIP4Address *addr;
			nm_auto_nmpobj NMPObject *r = NULL;
			NMPlatformIP4Route *route;
			in_addr_t network;

			if (!o)
				continue;

			addr = NMP_OBJECT_CAST_IP4_ADDRESS (o);
			if (addr->plen == 0)
				continue;

			nm_assert (addr->plen <= 32);

			/* The destination network depends on the peer-address. */
			network = nm_utils_ip4_address_clear_host_address (addr->peer_address, addr->plen);

			if (_ipv4_is_zeronet (network)) {
				/* Kernel doesn't add device-routes for destinations that
				 * start with 0.x.y.z. Skip them. */
				continue;
			}

			r = nmp_object_new (NMP_OBJECT_TYPE_IP4_ROUTE, NULL);
			route = NMP_OBJECT_CAST_IP4_ROUTE (r);

			route->ifindex = ifindex;
			route->rt_source = NM_IP_CONFIG_SOURCE_KERNEL;
			route->network = network;
			route->plen = addr->plen;
			route->pref_src = addr->address;
			route->metric = default_route_metric;
			route->scope_inv = nm_platform_route_scope_inv (NM_RT_SCOPE_LINK);

			nm_platform_ip_route_normalize (AF_INET, (NMPlatformIPRoute *) route);

			if (_lookup_route (self,
			                   r,
			                   NM_PLATFORM_IP_ROUTE_CMP_TYPE_ID)) {
				/* we already track this route. Don't add it again. */
			} else {
				if (!routes)
					routes = g_ptr_array_new_with_free_func ((GDestroyNotify) nmp_object_unref);
				g_ptr_array_add (routes, (gpointer) nmp_object_ref (r));
			}

			if (default_route_metric != NM_PLATFORM_ROUTE_METRIC_IP4_DEVICE_ROUTE) {
				nm_auto_nmpobj NMPObject *r_dev = NULL;

				r_dev = nmp_object_clone (r, FALSE);
				route = NMP_OBJECT_CAST_IP4_ROUTE (r_dev);
				route->metric = NM_PLATFORM_ROUTE_METRIC_IP4_DEVICE_ROUTE;

				nm_platform_ip_route_normalize (AF_INET, (NMPlatformIPRoute *) route);

				if (_lookup_route (self,
				                   r_dev,
				                   NM_PLATFORM_IP_ROUTE_CMP_TYPE_ID)) {
					/* we track such a route explicitly. Don't blacklist it. */
				} else {
					if (!ip4_dev_route_blacklist)
						ip4_dev_route_blacklist = g_ptr_array_new_with_free_func ((GDestroyNotify) nmp_object_unref);

					g_ptr_array_add (ip4_dev_route_blacklist,
					                 g_steal_pointer (&r_dev));
				}
			}
		}
	}

	nm_platform_ip4_address_sync (platform, ifindex, addresses);

	if (!nm_platform_ip_route_sync (platform,
	                                AF_INET,
	                                ifindex,
	                                routes,
	                                nm_platform_lookup_predicate_routes_main_skip_rtprot_kernel,
	                                NULL))
		success = FALSE;

	nm_platform_ip4_dev_route_blacklist_set (platform,
	                                         ifindex,
	                                         ip4_dev_route_blacklist);

	return success;
}

static void
merge_route_attributes (NMIPRoute *s_route, NMPlatformIP4Route *r)
{
	GVariant *variant;
	in_addr_t addr;

#define GET_ATTR(name, field, variant_type, type) \
	variant = nm_ip_route_get_attribute (s_route, name); \
	if (variant && g_variant_is_of_type (variant, G_VARIANT_TYPE_ ## variant_type)) \
		r->field = g_variant_get_ ## type (variant);

	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_TOS,            tos,            BYTE,     byte);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_WINDOW,         window,         UINT32,   uint32);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_CWND,           cwnd,           UINT32,   uint32);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_INITCWND,       initcwnd,       UINT32,   uint32);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_INITRWND,       initrwnd,       UINT32,   uint32);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_MTU,            mtu,            UINT32,   uint32);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_LOCK_WINDOW,    lock_window,    BOOLEAN,  boolean);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_LOCK_CWND,      lock_cwnd,      BOOLEAN,  boolean);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_LOCK_INITCWND,  lock_initcwnd,  BOOLEAN,  boolean);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_LOCK_INITRWND,  lock_initrwnd,  BOOLEAN,  boolean);
	GET_ATTR (NM_IP_ROUTE_ATTRIBUTE_LOCK_MTU,       lock_mtu,       BOOLEAN,  boolean);

	if (   (variant = nm_ip_route_get_attribute (s_route, NM_IP_ROUTE_ATTRIBUTE_SRC))
	    && g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING)) {
		if (inet_pton (AF_INET, g_variant_get_string (variant, NULL), &addr) == 1)
			r->pref_src = addr;
	}

#undef GET_ATTR
}

void
nm_ip4_config_merge_setting (NMIP4Config *self, NMSettingIPConfig *setting, guint32 default_route_metric)
{
	NMIP4ConfigPrivate *priv;
	guint naddresses, nroutes, nnameservers, nsearches;
	int i, priority;

	if (!setting)
		return;

	g_return_if_fail (NM_IS_SETTING_IP4_CONFIG (setting));

	priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_object_freeze_notify (G_OBJECT (self));

	naddresses = nm_setting_ip_config_get_num_addresses (setting);
	nroutes = nm_setting_ip_config_get_num_routes (setting);
	nnameservers = nm_setting_ip_config_get_num_dns (setting);
	nsearches = nm_setting_ip_config_get_num_dns_searches (setting);

	/* Gateway */
	if (nm_setting_ip_config_get_never_default (setting))
		nm_ip4_config_set_never_default (self, TRUE);
	else if (nm_setting_ip_config_get_ignore_auto_routes (setting))
		nm_ip4_config_set_never_default (self, FALSE);
	if (nm_setting_ip_config_get_gateway (setting)) {
		guint32 gateway;

		inet_pton (AF_INET, nm_setting_ip_config_get_gateway (setting), &gateway);
		nm_ip4_config_set_gateway (self, gateway);
	}

	if (priv->route_metric  == -1)
		priv->route_metric = nm_setting_ip_config_get_route_metric (setting);

	/* Addresses */
	for (i = 0; i < naddresses; i++) {
		NMIPAddress *s_addr = nm_setting_ip_config_get_address (setting, i);
		GVariant *label;
		NMPlatformIP4Address address;

		memset (&address, 0, sizeof (address));
		nm_ip_address_get_address_binary (s_addr, &address.address);
		address.peer_address = address.address;
		address.plen = nm_ip_address_get_prefix (s_addr);
		nm_assert (address.plen <= 32);
		address.lifetime = NM_PLATFORM_LIFETIME_PERMANENT;
		address.preferred = NM_PLATFORM_LIFETIME_PERMANENT;
		address.addr_source = NM_IP_CONFIG_SOURCE_USER;

		label = nm_ip_address_get_attribute (s_addr, "label");
		if (label)
			g_strlcpy (address.label, g_variant_get_string (label, NULL), sizeof (address.label));

		_add_address (self, NULL, &address);
	}

	/* Routes */
	if (nm_setting_ip_config_get_ignore_auto_routes (setting))
		nm_ip4_config_reset_routes (self);
	for (i = 0; i < nroutes; i++) {
		NMIPRoute *s_route = nm_setting_ip_config_get_route (setting, i);
		NMPlatformIP4Route route;

		if (nm_ip_route_get_family (s_route) != AF_INET) {
			nm_assert_not_reached ();
			continue;
		}

		memset (&route, 0, sizeof (route));
		nm_ip_route_get_dest_binary (s_route, &route.network);

		route.plen = nm_ip_route_get_prefix (s_route);
		nm_assert (route.plen <= 32);
		if (route.plen == 0)
			continue;

		nm_ip_route_get_next_hop_binary (s_route, &route.gateway);
		if (nm_ip_route_get_metric (s_route) == -1)
			route.metric = default_route_metric;
		else
			route.metric = nm_ip_route_get_metric (s_route);
		route.rt_source = NM_IP_CONFIG_SOURCE_USER;

		route.network = nm_utils_ip4_address_clear_host_address (route.network, route.plen);

		merge_route_attributes (s_route, &route);
		_add_route (self, NULL, &route);
	}

	/* DNS */
	if (nm_setting_ip_config_get_ignore_auto_dns (setting)) {
		nm_ip4_config_reset_nameservers (self);
		nm_ip4_config_reset_domains (self);
		nm_ip4_config_reset_searches (self);
	}
	for (i = 0; i < nnameservers; i++) {
		guint32 ip;

		if (inet_pton (AF_INET, nm_setting_ip_config_get_dns (setting, i), &ip) == 1)
			nm_ip4_config_add_nameserver (self, ip);
	}
	for (i = 0; i < nsearches; i++)
		nm_ip4_config_add_search (self, nm_setting_ip_config_get_dns_search (setting, i));

	i = 0;
	while ((i = nm_setting_ip_config_next_valid_dns_option (setting, i)) >= 0) {
		nm_ip4_config_add_dns_option (self, nm_setting_ip_config_get_dns_option (setting, i));
		i++;
	}

	priority = nm_setting_ip_config_get_dns_priority (setting);
	if (priority)
		nm_ip4_config_set_dns_priority (self, priority);

	g_object_thaw_notify (G_OBJECT (self));
}

NMSetting *
nm_ip4_config_create_setting (const NMIP4Config *self)
{
	NMSettingIPConfig *s_ip4;
	guint32 gateway;
	guint nnameservers, nsearches, noptions;
	const char *method = NULL;
	int i;
	gint64 route_metric;
	NMDedupMultiIter ipconf_iter;
	const NMPlatformIP4Address *address;
	const NMPlatformIP4Route *route;

	s_ip4 = NM_SETTING_IP_CONFIG (nm_setting_ip4_config_new ());

	if (!self) {
		g_object_set (s_ip4,
		              NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_DISABLED,
		              NULL);
		return NM_SETTING (s_ip4);
	}

	gateway = nm_ip4_config_get_gateway (self);
	nnameservers = nm_ip4_config_get_num_nameservers (self);
	nsearches = nm_ip4_config_get_num_searches (self);
	noptions = nm_ip4_config_get_num_dns_options (self);
	route_metric = nm_ip4_config_get_route_metric (self);

	/* Addresses */
	nm_ip_config_iter_ip4_address_for_each (&ipconf_iter, self, &address) {
		NMIPAddress *s_addr;

		/* Detect dynamic address */
		if (address->lifetime != NM_PLATFORM_LIFETIME_PERMANENT) {
			method = NM_SETTING_IP4_CONFIG_METHOD_AUTO;
			continue;
		}

		/* Static address found. */
		if (!method)
			method = NM_SETTING_IP4_CONFIG_METHOD_MANUAL;

		s_addr = nm_ip_address_new_binary (AF_INET, &address->address, address->plen, NULL);
		if (*address->label)
			nm_ip_address_set_attribute (s_addr, "label", g_variant_new_string (address->label));

		nm_setting_ip_config_add_address (s_ip4, s_addr);
		nm_ip_address_unref (s_addr);
	}

	/* Gateway */
	if (   nm_ip4_config_has_gateway (self)
	    && nm_setting_ip_config_get_num_addresses (s_ip4) > 0) {
		g_object_set (s_ip4,
		              NM_SETTING_IP_CONFIG_GATEWAY, nm_utils_inet4_ntop (gateway, NULL),
		              NULL);
	}

	/* Use 'disabled' if the method wasn't previously set */
	if (!method)
		method = NM_SETTING_IP4_CONFIG_METHOD_DISABLED;

	g_object_set (s_ip4,
	              NM_SETTING_IP_CONFIG_METHOD, method,
	              NM_SETTING_IP_CONFIG_ROUTE_METRIC, (gint64) route_metric,
	              NULL);

	/* Routes */
	nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, self, &route) {
		NMIPRoute *s_route;

		/* Ignore default route. */
		if (!route->plen)
			continue;

		/* Ignore routes provided by external sources */
		if (route->rt_source != nmp_utils_ip_config_source_round_trip_rtprot (NM_IP_CONFIG_SOURCE_USER))
			continue;

		s_route = nm_ip_route_new_binary (AF_INET,
		                                  &route->network, route->plen,
		                                  &route->gateway, route->metric,
		                                  NULL);
		nm_setting_ip_config_add_route (s_ip4, s_route);
		nm_ip_route_unref (s_route);
	}

	/* DNS */
	for (i = 0; i < nnameservers; i++) {
		guint32 nameserver = nm_ip4_config_get_nameserver (self, i);

		nm_setting_ip_config_add_dns (s_ip4, nm_utils_inet4_ntop (nameserver, NULL));
	}
	for (i = 0; i < nsearches; i++) {
		const char *search = nm_ip4_config_get_search (self, i);

		nm_setting_ip_config_add_dns_search (s_ip4, search);
	}

	for (i = 0; i < noptions; i++) {
		const char *option = nm_ip4_config_get_dns_option (self, i);

		nm_setting_ip_config_add_dns_option (s_ip4, option);
	}

	g_object_set (s_ip4,
	              NM_SETTING_IP_CONFIG_DNS_PRIORITY,
	              nm_ip4_config_get_dns_priority (self),
	              NULL);

	return NM_SETTING (s_ip4);
}

/*****************************************************************************/

void
nm_ip4_config_merge (NMIP4Config *dst, const NMIP4Config *src, NMIPConfigMergeFlags merge_flags)
{
	NMIP4ConfigPrivate *dst_priv;
	const NMIP4ConfigPrivate *src_priv;
	guint32 i;
	NMDedupMultiIter ipconf_iter;
	const NMPlatformIP4Address *address = NULL;

	g_return_if_fail (src != NULL);
	g_return_if_fail (dst != NULL);

	dst_priv = NM_IP4_CONFIG_GET_PRIVATE (dst);
	src_priv = NM_IP4_CONFIG_GET_PRIVATE (src);

	g_object_freeze_notify (G_OBJECT (dst));

	/* addresses */
	nm_ip_config_iter_ip4_address_for_each (&ipconf_iter, src, &address)
		_add_address (dst, NMP_OBJECT_UP_CAST (address), NULL);

	/* nameservers */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_DNS)) {
		for (i = 0; i < nm_ip4_config_get_num_nameservers (src); i++)
			nm_ip4_config_add_nameserver (dst, nm_ip4_config_get_nameserver (src, i));
	}

	/* default gateway */
	if (nm_ip4_config_has_gateway (src))
		nm_ip4_config_set_gateway (dst, nm_ip4_config_get_gateway (src));

	/* routes */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_ROUTES)) {
		const NMPlatformIP4Route *route;

		nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, src, &route)
			_add_route (dst, NMP_OBJECT_UP_CAST (route), NULL);
	}

	if (dst_priv->route_metric == -1)
		dst_priv->route_metric = src_priv->route_metric;
	else if (src_priv->route_metric != -1)
		dst_priv->route_metric = MIN (dst_priv->route_metric, src_priv->route_metric);

	/* domains */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_DNS)) {
		for (i = 0; i < nm_ip4_config_get_num_domains (src); i++)
			nm_ip4_config_add_domain (dst, nm_ip4_config_get_domain (src, i));
	}

	/* dns searches */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_DNS)) {
		for (i = 0; i < nm_ip4_config_get_num_searches (src); i++)
			nm_ip4_config_add_search (dst, nm_ip4_config_get_search (src, i));
	}

	/* dns options */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_DNS)) {
		for (i = 0; i < nm_ip4_config_get_num_dns_options (src); i++)
			nm_ip4_config_add_dns_option (dst, nm_ip4_config_get_dns_option (src, i));
	}

	/* MSS */
	if (nm_ip4_config_get_mss (src))
		nm_ip4_config_set_mss (dst, nm_ip4_config_get_mss (src));

	/* MTU */
	if (   src_priv->mtu_source > dst_priv->mtu_source
	    || (   src_priv->mtu_source == dst_priv->mtu_source
	        && (   (!dst_priv->mtu && src_priv->mtu)
	            || (dst_priv->mtu && src_priv->mtu < dst_priv->mtu))))
		nm_ip4_config_set_mtu (dst, src_priv->mtu, src_priv->mtu_source);

	/* NIS */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_DNS)) {
		for (i = 0; i < nm_ip4_config_get_num_nis_servers (src); i++)
			nm_ip4_config_add_nis_server (dst, nm_ip4_config_get_nis_server (src, i));

		if (nm_ip4_config_get_nis_domain (src))
			nm_ip4_config_set_nis_domain (dst, nm_ip4_config_get_nis_domain (src));
	}

	/* WINS */
	if (!NM_FLAGS_HAS (merge_flags, NM_IP_CONFIG_MERGE_NO_DNS)) {
		for (i = 0; i < nm_ip4_config_get_num_wins (src); i++)
			nm_ip4_config_add_wins (dst, nm_ip4_config_get_wins (src, i));
	}

	/* metered flag */
	nm_ip4_config_set_metered (dst, nm_ip4_config_get_metered (dst) ||
	                                nm_ip4_config_get_metered (src));

	/* DNS priority */
	if (nm_ip4_config_get_dns_priority (src))
		nm_ip4_config_set_dns_priority (dst, nm_ip4_config_get_dns_priority (src));

	g_object_thaw_notify (G_OBJECT (dst));
}

/*****************************************************************************/

static int
_nameservers_get_index (const NMIP4Config *self, guint32 ns)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	guint i;

	for (i = 0; i < priv->nameservers->len; i++) {
		guint32 n = g_array_index (priv->nameservers, guint32, i);

		if (ns == n)
			return (int) i;
	}
	return -1;
}

static int
_domains_get_index (const NMIP4Config *self, const char *domain)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	guint i;

	for (i = 0; i < priv->domains->len; i++) {
		const char *d = g_ptr_array_index (priv->domains, i);

		if (g_strcmp0 (domain, d) == 0)
			return (int) i;
	}
	return -1;
}

static int
_searches_get_index (const NMIP4Config *self, const char *search)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	guint i;

	for (i = 0; i < priv->searches->len; i++) {
		const char *s = g_ptr_array_index (priv->searches, i);

		if (g_strcmp0 (search, s) == 0)
			return (int) i;
	}
	return -1;
}

static int
_dns_options_get_index (const NMIP4Config *self, const char *option)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	guint i;

	for (i = 0; i < priv->dns_options->len; i++) {
		const char *s = g_ptr_array_index (priv->dns_options, i);

		if (g_strcmp0 (option, s) == 0)
			return (int) i;
	}
	return -1;
}

static int
_nis_servers_get_index (const NMIP4Config *self, guint32 nis_server)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	guint i;

	for (i = 0; i < priv->nis->len; i++) {
		guint32 n = g_array_index (priv->nis, guint32, i);

		if (n == nis_server)
			return (int) i;
	}
	return -1;
}

static int
_wins_get_index (const NMIP4Config *self, guint32 wins_server)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	guint i;

	for (i = 0; i < priv->wins->len; i++) {
		guint32 n = g_array_index (priv->wins, guint32, i);

		if (n == wins_server)
			return (int) i;
	}
	return -1;
}

/*****************************************************************************/

/**
 * nm_ip4_config_subtract:
 * @dst: config from which to remove everything in @src
 * @src: config to remove from @dst
 *
 * Removes everything in @src from @dst.
 */
void
nm_ip4_config_subtract (NMIP4Config *dst, const NMIP4Config *src)
{
	NMIP4ConfigPrivate *dst_priv;
	guint i;
	gint idx;
	const NMPlatformIP4Address *a;
	const NMPlatformIP4Route *r;
	NMDedupMultiIter ipconf_iter;
	gboolean changed;

	g_return_if_fail (src != NULL);
	g_return_if_fail (dst != NULL);

	dst_priv = NM_IP4_CONFIG_GET_PRIVATE (dst);

	g_object_freeze_notify (G_OBJECT (dst));

	/* addresses */
	changed = FALSE;
	nm_ip_config_iter_ip4_address_for_each (&ipconf_iter, src, &a) {
		if (nm_dedup_multi_index_remove_obj (dst_priv->multi_idx,
		                                     &dst_priv->idx_ip4_addresses,
		                                     NMP_OBJECT_UP_CAST (a),
		                                     NULL))
			changed = TRUE;
	}
	if (changed)
		_notify_addresses (dst);

	/* nameservers */
	for (i = 0; i < nm_ip4_config_get_num_nameservers (src); i++) {
		idx = _nameservers_get_index (dst, nm_ip4_config_get_nameserver (src, i));
		if (idx >= 0)
			nm_ip4_config_del_nameserver (dst, idx);
	}

	/* default gateway */
	if (   (nm_ip4_config_has_gateway (src) == nm_ip4_config_has_gateway (dst))
	    && (nm_ip4_config_get_gateway (src) == nm_ip4_config_get_gateway (dst)))
		nm_ip4_config_unset_gateway (dst);

	if (!nm_ip4_config_get_num_addresses (dst))
		nm_ip4_config_unset_gateway (dst);

	/* ignore route_metric */

	/* routes */
	changed = FALSE;
	nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, src, &r) {
		if (nm_dedup_multi_index_remove_obj (dst_priv->multi_idx,
		                                     &dst_priv->idx_ip4_routes,
		                                     NMP_OBJECT_UP_CAST (r),
		                                     NULL))
			changed = TRUE;
	}
	if (changed)
		_notify_routes (dst);

	/* domains */
	for (i = 0; i < nm_ip4_config_get_num_domains (src); i++) {
		idx = _domains_get_index (dst, nm_ip4_config_get_domain (src, i));
		if (idx >= 0)
			nm_ip4_config_del_domain (dst, idx);
	}

	/* dns searches */
	for (i = 0; i < nm_ip4_config_get_num_searches (src); i++) {
		idx = _searches_get_index (dst, nm_ip4_config_get_search (src, i));
		if (idx >= 0)
			nm_ip4_config_del_search (dst, idx);
	}

	/* dns options */
	for (i = 0; i < nm_ip4_config_get_num_dns_options (src); i++) {
		idx = _dns_options_get_index (dst, nm_ip4_config_get_dns_option (src, i));
		if (idx >= 0)
			nm_ip4_config_del_dns_option (dst, idx);
	}

	/* MSS */
	if (nm_ip4_config_get_mss (src) == nm_ip4_config_get_mss (dst))
		nm_ip4_config_set_mss (dst, 0);

	/* MTU */
	if (   nm_ip4_config_get_mtu (src) == nm_ip4_config_get_mtu (dst)
	    && nm_ip4_config_get_mtu_source (src) == nm_ip4_config_get_mtu_source (dst))
		nm_ip4_config_set_mtu (dst, 0, NM_IP_CONFIG_SOURCE_UNKNOWN);

	/* NIS */
	for (i = 0; i < nm_ip4_config_get_num_nis_servers (src); i++) {
		idx = _nis_servers_get_index (dst, nm_ip4_config_get_nis_server (src, i));
		if (idx >= 0)
			nm_ip4_config_del_nis_server (dst, idx);
	}

	if (g_strcmp0 (nm_ip4_config_get_nis_domain (src), nm_ip4_config_get_nis_domain (dst)) == 0)
		nm_ip4_config_set_nis_domain (dst, NULL);

	/* WINS */
	for (i = 0; i < nm_ip4_config_get_num_wins (src); i++) {
		idx = _wins_get_index (dst, nm_ip4_config_get_wins (src, i));
		if (idx >= 0)
			nm_ip4_config_del_wins (dst, idx);
	}

	/* DNS priority */
	if (nm_ip4_config_get_dns_priority (src) == nm_ip4_config_get_dns_priority (dst))
		nm_ip4_config_set_dns_priority (dst, 0);

	g_object_thaw_notify (G_OBJECT (dst));
}

void
nm_ip4_config_intersect (NMIP4Config *dst, const NMIP4Config *src)
{
	NMIP4ConfigPrivate *dst_priv;
	const NMIP4ConfigPrivate *src_priv;
	NMDedupMultiIter ipconf_iter;
	const NMPlatformIP4Address *a;
	const NMPlatformIP4Route *r;
	gboolean changed;

	g_return_if_fail (src);
	g_return_if_fail (dst);

	g_object_freeze_notify (G_OBJECT (dst));

	dst_priv = NM_IP4_CONFIG_GET_PRIVATE (dst);
	src_priv = NM_IP4_CONFIG_GET_PRIVATE (src);

	/* addresses */
	changed = FALSE;
	nm_ip_config_iter_ip4_address_for_each (&ipconf_iter, dst, &a) {
		if (nm_dedup_multi_index_lookup_obj (src_priv->multi_idx,
		                                     &src_priv->idx_ip4_addresses,
		                                     NMP_OBJECT_UP_CAST (a)))
			continue;

		if (nm_dedup_multi_index_remove_entry (dst_priv->multi_idx,
		                                       ipconf_iter.current) != 1)
			nm_assert_not_reached ();
		changed = TRUE;
	}
	if (changed)
		_notify_addresses (dst);

	/* ignore route_metric */
	/* ignore nameservers */

	/* default gateway */
	if (   !nm_ip4_config_get_num_addresses (dst)
	    || (nm_ip4_config_has_gateway (src) != nm_ip4_config_has_gateway (dst))
	    || (nm_ip4_config_get_gateway (src) != nm_ip4_config_get_gateway (dst))) {
		nm_ip4_config_unset_gateway (dst);
	}

	/* routes */
	changed = FALSE;
	nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, dst, &r) {
		if (nm_dedup_multi_index_lookup_obj (src_priv->multi_idx,
		                                     &src_priv->idx_ip4_routes,
		                                     NMP_OBJECT_UP_CAST (r)))
			continue;

		if (nm_dedup_multi_index_remove_entry (dst_priv->multi_idx,
		                                       ipconf_iter.current) != 1)
			nm_assert_not_reached ();
		changed = TRUE;
	}
	if (changed)
		_notify_routes (dst);

	/* ignore domains */
	/* ignore dns searches */
	/* ignore dns options */
	/* ignore NIS */
	/* ignore WINS */

	g_object_thaw_notify (G_OBJECT (dst));
}


/**
 * nm_ip4_config_replace:
 * @dst: config to replace with @src content
 * @src: source config to copy
 * @relevant_changes: return whether there are changes to the
 * destination object that are relevant. This is equal to
 * nm_ip4_config_equal() showing any difference.
 *
 * Replaces everything in @dst with @src so that the two configurations
 * contain the same content -- with the exception of the dbus path.
 *
 * Returns: whether the @dst instance changed in any way (including minor changes,
 * that are not signaled by the output parameter @relevant_changes).
 */
gboolean
nm_ip4_config_replace (NMIP4Config *dst, const NMIP4Config *src, gboolean *relevant_changes)
{
#if NM_MORE_ASSERTS
	gboolean config_equal;
#endif
	gboolean has_minor_changes = FALSE, has_relevant_changes = FALSE, are_equal;
	guint i, num;
	NMIP4ConfigPrivate *dst_priv;
	const NMIP4ConfigPrivate *src_priv;
	NMDedupMultiIter ipconf_iter_src, ipconf_iter_dst;
	const NMDedupMultiHeadEntry *head_entry_src;

	g_return_val_if_fail (src != NULL, FALSE);
	g_return_val_if_fail (dst != NULL, FALSE);
	g_return_val_if_fail (src != dst, FALSE);

#if NM_MORE_ASSERTS
	config_equal = nm_ip4_config_equal (dst, src);
#endif

	dst_priv = NM_IP4_CONFIG_GET_PRIVATE (dst);
	src_priv = NM_IP4_CONFIG_GET_PRIVATE (src);

	g_object_freeze_notify (G_OBJECT (dst));

	/* ifindex */
	if (src_priv->ifindex != dst_priv->ifindex) {
		dst_priv->ifindex = src_priv->ifindex;
		has_minor_changes = TRUE;
	}

	/* never_default */
	if (src_priv->never_default != dst_priv->never_default) {
		dst_priv->never_default = src_priv->never_default;
		has_minor_changes = TRUE;
	}

	/* default gateway */
	if (   src_priv->gateway != dst_priv->gateway
	    || src_priv->has_gateway != dst_priv->has_gateway) {
		if (src_priv->has_gateway)
			nm_ip4_config_set_gateway (dst, src_priv->gateway);
		else
			nm_ip4_config_unset_gateway (dst);
		has_relevant_changes = TRUE;
	}

	if (src_priv->route_metric != dst_priv->route_metric) {
		dst_priv->route_metric = src_priv->route_metric;
		has_minor_changes = TRUE;
	}

	/* addresses */
	head_entry_src = nm_ip4_config_lookup_addresses (src);
	nm_dedup_multi_iter_init (&ipconf_iter_src, head_entry_src);
	nm_ip_config_iter_ip4_address_init (&ipconf_iter_dst, dst);
	are_equal = TRUE;
	while (TRUE) {
		gboolean has;
		const NMPlatformIP4Address *r_src = NULL;
		const NMPlatformIP4Address *r_dst = NULL;

		has = nm_ip_config_iter_ip4_address_next (&ipconf_iter_src, &r_src);
		if (has != nm_ip_config_iter_ip4_address_next (&ipconf_iter_dst, &r_dst)) {
			are_equal = FALSE;
			has_relevant_changes = TRUE;
			break;
		}
		if (!has)
			break;

		if (nm_platform_ip4_address_cmp (r_src, r_dst) != 0) {
			are_equal = FALSE;
			if (   !nm_ip_config_obj_id_equal_ip4_address (r_src, r_dst)
			    || r_src->peer_address != r_dst->peer_address) {
				has_relevant_changes = TRUE;
				break;
			}
		}
	}
	if (!are_equal) {
		has_minor_changes = TRUE;
		nm_dedup_multi_index_dirty_set_idx (dst_priv->multi_idx, &dst_priv->idx_ip4_addresses);
		nm_dedup_multi_iter_for_each (&ipconf_iter_src, head_entry_src) {
			_nm_ip_config_add_obj (dst_priv->multi_idx,
			                       &dst_priv->idx_ip4_addresses_,
			                       dst_priv->ifindex,
			                       ipconf_iter_src.current->obj,
			                       NULL,
			                       FALSE,
			                       TRUE,
			                       NULL);
		}
		nm_dedup_multi_index_dirty_remove_idx (dst_priv->multi_idx, &dst_priv->idx_ip4_addresses, FALSE);
		_notify_addresses (dst);
	}

	/* routes */
	head_entry_src = nm_ip4_config_lookup_routes (src);
	nm_dedup_multi_iter_init (&ipconf_iter_src, head_entry_src);
	nm_ip_config_iter_ip4_route_init (&ipconf_iter_dst, dst);
	are_equal = TRUE;
	while (TRUE) {
		gboolean has;
		const NMPlatformIP4Route *r_src = NULL;
		const NMPlatformIP4Route *r_dst = NULL;

		has = nm_ip_config_iter_ip4_route_next (&ipconf_iter_src, &r_src);
		if (has != nm_ip_config_iter_ip4_route_next (&ipconf_iter_dst, &r_dst)) {
			are_equal = FALSE;
			has_relevant_changes = TRUE;
			break;
		}
		if (!has)
			break;

		if (nm_platform_ip4_route_cmp_full (r_src, r_dst) != 0) {
			are_equal = FALSE;
			if (   !nm_ip_config_obj_id_equal_ip4_route (r_src, r_dst)
			    || r_src->gateway != r_dst->gateway
			    || r_src->metric != r_dst->metric) {
				has_relevant_changes = TRUE;
				break;
			}
		}
	}
	if (!are_equal) {
		has_minor_changes = TRUE;
		nm_dedup_multi_index_dirty_set_idx (dst_priv->multi_idx, &dst_priv->idx_ip4_routes);
		nm_dedup_multi_iter_for_each (&ipconf_iter_src, head_entry_src) {
			_nm_ip_config_add_obj (dst_priv->multi_idx,
			                       &dst_priv->idx_ip4_routes_,
			                       dst_priv->ifindex,
			                       ipconf_iter_src.current->obj,
			                       NULL,
			                       FALSE,
			                       TRUE,
			                       NULL);
		}
		nm_dedup_multi_index_dirty_remove_idx (dst_priv->multi_idx, &dst_priv->idx_ip4_routes, FALSE);
		_notify_routes (dst);
	}

	/* nameservers */
	num = nm_ip4_config_get_num_nameservers (src);
	are_equal = num == nm_ip4_config_get_num_nameservers (dst);
	if (are_equal) {
		for (i = 0; i < num; i++ ) {
			if (nm_ip4_config_get_nameserver (src, i) != nm_ip4_config_get_nameserver (dst, i)) {
				are_equal = FALSE;
				break;
			}
		}
	}
	if (!are_equal) {
		nm_ip4_config_reset_nameservers (dst);
		for (i = 0; i < num; i++)
			nm_ip4_config_add_nameserver (dst, nm_ip4_config_get_nameserver (src, i));
		has_relevant_changes = TRUE;
	}

	/* domains */
	num = nm_ip4_config_get_num_domains (src);
	are_equal = num == nm_ip4_config_get_num_domains (dst);
	if (are_equal) {
		for (i = 0; i < num; i++ ) {
			if (g_strcmp0 (nm_ip4_config_get_domain (src, i),
			               nm_ip4_config_get_domain (dst, i))) {
				are_equal = FALSE;
				break;
			}
		}
	}
	if (!are_equal) {
		nm_ip4_config_reset_domains (dst);
		for (i = 0; i < num; i++)
			nm_ip4_config_add_domain (dst, nm_ip4_config_get_domain (src, i));
		has_relevant_changes = TRUE;
	}

	/* dns searches */
	num = nm_ip4_config_get_num_searches (src);
	are_equal = num == nm_ip4_config_get_num_searches (dst);
	if (are_equal) {
		for (i = 0; i < num; i++ ) {
			if (g_strcmp0 (nm_ip4_config_get_search (src, i),
			               nm_ip4_config_get_search (dst, i))) {
				are_equal = FALSE;
				break;
			}
		}
	}
	if (!are_equal) {
		nm_ip4_config_reset_searches (dst);
		for (i = 0; i < num; i++)
			nm_ip4_config_add_search (dst, nm_ip4_config_get_search (src, i));
		has_relevant_changes = TRUE;
	}

	/* dns options */
	num = nm_ip4_config_get_num_dns_options (src);
	are_equal = num == nm_ip4_config_get_num_dns_options (dst);
	if (are_equal) {
		for (i = 0; i < num; i++ ) {
			if (g_strcmp0 (nm_ip4_config_get_dns_option (src, i),
			               nm_ip4_config_get_dns_option (dst, i))) {
				are_equal = FALSE;
				break;
			}
		}
	}
	if (!are_equal) {
		nm_ip4_config_reset_dns_options (dst);
		for (i = 0; i < num; i++)
			nm_ip4_config_add_dns_option (dst, nm_ip4_config_get_dns_option (src, i));
		has_relevant_changes = TRUE;
	}

	/* DNS priority */
	if (src_priv->dns_priority != dst_priv->dns_priority) {
		nm_ip4_config_set_dns_priority (dst, src_priv->dns_priority);
		has_minor_changes = TRUE;
	}

	/* mss */
	if (src_priv->mss != dst_priv->mss) {
		nm_ip4_config_set_mss (dst, src_priv->mss);
		has_minor_changes = TRUE;
	}

	/* nis */
	num = nm_ip4_config_get_num_nis_servers (src);
	are_equal = num == nm_ip4_config_get_num_nis_servers (dst);
	if (are_equal) {
		for (i = 0; i < num; i++ ) {
			if (nm_ip4_config_get_nis_server (src, i) != nm_ip4_config_get_nis_server (dst, i)) {
				are_equal = FALSE;
				break;
			}
		}
	}
	if (!are_equal) {
		nm_ip4_config_reset_nis_servers (dst);
		for (i = 0; i < num; i++)
			nm_ip4_config_add_nis_server (dst, nm_ip4_config_get_nis_server (src, i));
		has_relevant_changes = TRUE;
	}

	/* nis_domain */
	if (g_strcmp0 (src_priv->nis_domain, dst_priv->nis_domain)) {
		nm_ip4_config_set_nis_domain (dst, src_priv->nis_domain);
		has_relevant_changes = TRUE;
	}

	/* wins */
	num = nm_ip4_config_get_num_wins (src);
	are_equal = num == nm_ip4_config_get_num_wins (dst);
	if (are_equal) {
		for (i = 0; i < num; i++ ) {
			if (nm_ip4_config_get_wins (src, i) != nm_ip4_config_get_wins (dst, i)) {
				are_equal = FALSE;
				break;
			}
		}
	}
	if (!are_equal) {
		nm_ip4_config_reset_wins (dst);
		for (i = 0; i < num; i++)
			nm_ip4_config_add_wins (dst, nm_ip4_config_get_wins (src, i));
		has_relevant_changes = TRUE;
	}

	/* mtu */
	if (   src_priv->mtu != dst_priv->mtu
	    || src_priv->mtu_source != dst_priv->mtu_source) {
		nm_ip4_config_set_mtu (dst, src_priv->mtu, src_priv->mtu_source);
		has_minor_changes = TRUE;
	}

	/* metered */
	if (src_priv->metered != dst_priv->metered) {
		dst_priv->metered = src_priv->metered;
		has_minor_changes = TRUE;
	}

#if NM_MORE_ASSERTS
	/* config_equal does not compare *all* the fields, therefore, we might have has_minor_changes
	 * regardless of config_equal. But config_equal must correspond to has_relevant_changes. */
	nm_assert (config_equal == !has_relevant_changes);
#endif

	g_object_thaw_notify (G_OBJECT (dst));

	if (relevant_changes)
		*relevant_changes = has_relevant_changes;

	return has_relevant_changes || has_minor_changes;
}

void
nm_ip4_config_dump (const NMIP4Config *self, const char *detail)
{
	guint32 tmp;
	guint i;
	const char *str;
	NMDedupMultiIter ipconf_iter;
	const NMPlatformIP4Address *address;
	const NMPlatformIP4Route *route;

	g_message ("--------- NMIP4Config %p (%s)", self, detail);

	if (self == NULL) {
		g_message (" (null)");
		return;
	}

	str = nm_exported_object_get_path (NM_EXPORTED_OBJECT (self));
	if (str)
		g_message ("   path: %s", str);

	/* addresses */
	nm_ip_config_iter_ip4_address_for_each (&ipconf_iter, self, &address)
		g_message ("      a: %s", nm_platform_ip4_address_to_string (address, NULL, 0));

	/* default gateway */
	if (nm_ip4_config_has_gateway (self)) {
		tmp = nm_ip4_config_get_gateway (self);
		g_message ("     gw: %s", nm_utils_inet4_ntop (tmp, NULL));
	}

	/* nameservers */
	for (i = 0; i < nm_ip4_config_get_num_nameservers (self); i++) {
		tmp = nm_ip4_config_get_nameserver (self, i);
		g_message ("     ns: %s", nm_utils_inet4_ntop (tmp, NULL));
	}

	/* routes */
	nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, self, &route)
		g_message ("     rt: %s", nm_platform_ip4_route_to_string (route, NULL, 0));

	/* domains */
	for (i = 0; i < nm_ip4_config_get_num_domains (self); i++)
		g_message (" domain: %s", nm_ip4_config_get_domain (self, i));

	/* dns searches */
	for (i = 0; i < nm_ip4_config_get_num_searches (self); i++)
		g_message (" search: %s", nm_ip4_config_get_search (self, i));

	/* dns options */
	for (i = 0; i < nm_ip4_config_get_num_dns_options (self); i++)
		g_message (" dnsopt: %s", nm_ip4_config_get_dns_option (self, i));

	g_message (" dnspri: %d", nm_ip4_config_get_dns_priority (self));

	g_message ("    mss: %"G_GUINT32_FORMAT, nm_ip4_config_get_mss (self));
	g_message ("    mtu: %"G_GUINT32_FORMAT" (source: %d)", nm_ip4_config_get_mtu (self), (int) nm_ip4_config_get_mtu_source (self));

	/* NIS */
	for (i = 0; i < nm_ip4_config_get_num_nis_servers (self); i++) {
		tmp = nm_ip4_config_get_nis_server (self, i);
		g_message ("    nis: %s", nm_utils_inet4_ntop (tmp, NULL));
	}

	g_message (" nisdmn: %s", nm_ip4_config_get_nis_domain (self) ?: "(none)");

	/* WINS */
	for (i = 0; i < nm_ip4_config_get_num_wins (self); i++) {
		tmp = nm_ip4_config_get_wins (self, i);
		g_message ("   wins: %s", nm_utils_inet4_ntop (tmp, NULL));
	}

	g_message (" n-dflt: %d", nm_ip4_config_get_never_default (self));
	g_message (" mtrd:   %d", (int) nm_ip4_config_get_metered (self));
}

gboolean
nm_ip4_config_destination_is_direct (const NMIP4Config *self, guint32 network, guint8 plen)
{
	const NMPlatformIP4Address *item;
	in_addr_t peer_network;
	NMDedupMultiIter iter;

	nm_ip_config_iter_ip4_address_for_each (&iter, self, &item) {
		if (item->plen > plen)
			continue;

		peer_network = nm_utils_ip4_address_clear_host_address (item->peer_address, item->plen);
		if (_ipv4_is_zeronet (peer_network))
			continue;

		if (peer_network != nm_utils_ip4_address_clear_host_address (network, item->plen))
			continue;

		return TRUE;
	}

	return FALSE;
}

/*****************************************************************************/

void
nm_ip4_config_set_never_default (NMIP4Config *self, gboolean never_default)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	priv->never_default = never_default;
}

gboolean
nm_ip4_config_get_never_default (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->never_default;
}

void
nm_ip4_config_set_gateway (NMIP4Config *self, guint32 gateway)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->gateway != gateway || !priv->has_gateway) {
		priv->gateway = gateway;
		priv->has_gateway = TRUE;
		_notify (self, PROP_GATEWAY);
	}
}

void
nm_ip4_config_unset_gateway (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->has_gateway) {
		priv->gateway = 0;
		priv->has_gateway = FALSE;
		_notify (self, PROP_GATEWAY);
	}
}

/**
 * nm_ip4_config_has_gateway:
 * @self: the #NMIP4Config object
 *
 * NetworkManager's handling of default-routes is limited and usually a default-route
 * cannot have gateway 0.0.0.0. For peer-to-peer routes, we still want to
 * support that, so we need to differenciate between no-default-route and a
 * on-link-default route. Hence nm_ip4_config_has_gateway().
 *
 * Returns: whether the object has a gateway explicitly set. */
gboolean
nm_ip4_config_has_gateway (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->has_gateway;
}

guint32
nm_ip4_config_get_gateway (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->gateway;
}

gint64
nm_ip4_config_get_route_metric (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->route_metric;
}

/*****************************************************************************/

void
nm_ip4_config_reset_addresses (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (nm_dedup_multi_index_remove_idx (priv->multi_idx,
	                                     &priv->idx_ip4_addresses) > 0)
		_notify_addresses (self);
}

static void
_add_address (NMIP4Config *self, const NMPObject *obj_new, const NMPlatformIP4Address *new)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (_nm_ip_config_add_obj (priv->multi_idx,
	                           &priv->idx_ip4_addresses_,
	                           priv->ifindex,
	                           obj_new,
	                           (const NMPlatformObject *) new,
	                           TRUE,
	                           FALSE,
	                           NULL))
		_notify_addresses (self);
}

/**
 * nm_ip4_config_add_address:
 * @self: the #NMIP4Config
 * @new: the new address to add to @self
 *
 * Adds the new address to @self.  If an address with the same basic properties
 * (address, prefix) already exists in @self, it is overwritten with the
 * lifetime and preferred of @new.  The source is also overwritten by the source
 * from @new if that source is higher priority.
 */
void
nm_ip4_config_add_address (NMIP4Config *self, const NMPlatformIP4Address *new)
{
	g_return_if_fail (self);
	g_return_if_fail (new);
	g_return_if_fail (new->plen > 0 && new->plen <= 32);
	g_return_if_fail (NM_IP4_CONFIG_GET_PRIVATE (self)->ifindex > 0);

	_add_address (self, NULL, new);
}

void
_nmtst_nm_ip4_config_del_address (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	const NMPlatformIP4Address *a;

	a = _nmtst_nm_ip4_config_get_address (self, i);
	g_return_if_fail (a);

	if (nm_dedup_multi_index_remove_obj (priv->multi_idx,
	                                     &priv->idx_ip4_addresses,
	                                     NMP_OBJECT_UP_CAST (a),
	                                     NULL) != 1)
		g_return_if_reached ();
	_notify_addresses (self);
}

guint
nm_ip4_config_get_num_addresses (const NMIP4Config *self)
{
	const NMDedupMultiHeadEntry *head_entry;

	head_entry = nm_ip4_config_lookup_addresses (self);
	return head_entry ? head_entry->len : 0;
}

const NMPlatformIP4Address *
nm_ip4_config_get_first_address (const NMIP4Config *self)
{
	NMDedupMultiIter iter;
	const NMPlatformIP4Address *a = NULL;

	nm_ip_config_iter_ip4_address_for_each (&iter, self, &a)
		return a;
	return NULL;
}

const NMPlatformIP4Address *
_nmtst_nm_ip4_config_get_address (const NMIP4Config *self, guint i)
{
	NMDedupMultiIter iter;
	const NMPlatformIP4Address *a = NULL;
	guint j;

	j = 0;
	nm_ip_config_iter_ip4_address_for_each (&iter, self, &a) {
		if (i == j)
			return a;
		j++;
	}
	g_return_val_if_reached (NULL);
}

gboolean
nm_ip4_config_address_exists (const NMIP4Config *self,
                              const NMPlatformIP4Address *needle)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	NMPObject obj_stack;

	nmp_object_stackinit_id_ip4_address (&obj_stack,
	                                     priv->ifindex,
	                                     needle->address,
	                                     needle->plen,
	                                     needle->peer_address);
	return !!nm_dedup_multi_index_lookup_obj (priv->multi_idx,
	                                          &priv->idx_ip4_addresses,
	                                          &obj_stack);
}

/*****************************************************************************/

static const NMDedupMultiEntry *
_lookup_route (const NMIP4Config *self,
               const NMPObject *needle,
               NMPlatformIPRouteCmpType cmp_type)
{
	const NMIP4ConfigPrivate *priv;

	nm_assert (NM_IS_IP4_CONFIG (self));
	nm_assert (NMP_OBJECT_GET_TYPE (needle) == NMP_OBJECT_TYPE_IP4_ROUTE);

	priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return _nm_ip_config_lookup_ip_route (priv->multi_idx,
	                                      &priv->idx_ip4_routes_,
	                                      needle,
	                                      cmp_type);
}

void
nm_ip4_config_reset_routes (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (nm_dedup_multi_index_remove_idx (priv->multi_idx,
	                                     &priv->idx_ip4_routes) > 0)
		_notify_routes (self);
}

static void
_add_route (NMIP4Config *self, const NMPObject *obj_new, const NMPlatformIP4Route *new)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	nm_assert ((!new) != (!obj_new));
	nm_assert (!new || _route_valid (new));
	nm_assert (!obj_new || _route_valid (NMP_OBJECT_CAST_IP4_ROUTE (obj_new)));

	if (_nm_ip_config_add_obj (priv->multi_idx,
	                           &priv->idx_ip4_routes_,
	                           priv->ifindex,
	                           obj_new,
	                           (const NMPlatformObject *) new,
	                           TRUE,
	                           FALSE,
	                           NULL))
		_notify_routes (self);
}

/**
 * nm_ip4_config_add_route:
 * @self: the #NMIP4Config
 * @new: the new route to add to @self
 *
 * Adds the new route to @self.  If a route with the same basic properties
 * (network, prefix) already exists in @self, it is overwritten including the
 * gateway and metric of @new.  The source is also overwritten by the source
 * from @new if that source is higher priority.
 */
void
nm_ip4_config_add_route (NMIP4Config *self, const NMPlatformIP4Route *new)
{
	g_return_if_fail (self);
	g_return_if_fail (new);
	g_return_if_fail (new->plen > 0 && new->plen <= 32);
	g_return_if_fail (NM_IP4_CONFIG_GET_PRIVATE (self)->ifindex > 0);

	_add_route (self, NULL, new);
}

void
_nmtst_nm_ip4_config_del_route (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	const NMPlatformIP4Route *r;

	r = _nmtst_nm_ip4_config_get_route (self, i);
	g_return_if_fail (r);

	if (nm_dedup_multi_index_remove_obj (priv->multi_idx,
	                                     &priv->idx_ip4_routes,
	                                     NMP_OBJECT_UP_CAST (r),
	                                     NULL) != 1)
		g_return_if_reached ();
	_notify_routes (self);
}

guint
nm_ip4_config_get_num_routes (const NMIP4Config *self)
{
	const NMDedupMultiHeadEntry *head_entry;

	head_entry = nm_ip4_config_lookup_routes (self);
	nm_assert ((head_entry ? head_entry->len : 0) == c_list_length (&head_entry->lst_entries_head));
	return head_entry ? head_entry->len : 0;
}

const NMPlatformIP4Route *
_nmtst_nm_ip4_config_get_route (const NMIP4Config *self, guint i)
{
	NMDedupMultiIter iter;
	const NMPlatformIP4Route *r = NULL;
	guint j;

	j = 0;
	nm_ip_config_iter_ip4_route_for_each (&iter, self, &r) {
		if (i == j)
			return r;
		j++;
	}
	g_return_val_if_reached (NULL);
}

const NMPlatformIP4Route *
nm_ip4_config_get_direct_route_for_host (const NMIP4Config *self, guint32 host)
{
	const NMPlatformIP4Route *best_route = NULL;
	const NMPlatformIP4Route *item;
	NMDedupMultiIter ipconf_iter;

	g_return_val_if_fail (host, NULL);

	nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, self, &item) {
		if (item->gateway != 0)
			continue;

		if (best_route && best_route->plen > item->plen)
			continue;

		if (nm_utils_ip4_address_clear_host_address (host, item->plen) != nm_utils_ip4_address_clear_host_address (item->network, item->plen))
			continue;

		if (best_route && best_route->metric <= item->metric)
			continue;

		best_route = item;
	}
	return best_route;
}

/*****************************************************************************/

void
nm_ip4_config_reset_nameservers (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->nameservers->len != 0) {
		g_array_set_size (priv->nameservers, 0);
		_notify (self, PROP_NAMESERVERS);
	}
}

void
nm_ip4_config_add_nameserver (NMIP4Config *self, guint32 new)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	int i;

	g_return_if_fail (new != 0);

	for (i = 0; i < priv->nameservers->len; i++)
		if (new == g_array_index (priv->nameservers, guint32, i))
			return;

	g_array_append_val (priv->nameservers, new);
	_notify (self, PROP_NAMESERVERS);
}

void
nm_ip4_config_del_nameserver (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_return_if_fail (i < priv->nameservers->len);

	g_array_remove_index (priv->nameservers, i);
	_notify (self, PROP_NAMESERVERS);
}

guint
nm_ip4_config_get_num_nameservers (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->nameservers->len;
}

guint32
nm_ip4_config_get_nameserver (const NMIP4Config *self, guint i)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return g_array_index (priv->nameservers, guint32, i);
}

/*****************************************************************************/

void
nm_ip4_config_reset_domains (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->domains->len != 0) {
		g_ptr_array_set_size (priv->domains, 0);
		_notify (self, PROP_DOMAINS);
	}
}

void
nm_ip4_config_add_domain (NMIP4Config *self, const char *domain)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	int i;

	g_return_if_fail (domain != NULL);
	g_return_if_fail (domain[0] != '\0');

	for (i = 0; i < priv->domains->len; i++)
		if (!g_strcmp0 (g_ptr_array_index (priv->domains, i), domain))
			return;

	g_ptr_array_add (priv->domains, g_strdup (domain));
	_notify (self, PROP_DOMAINS);
}

void
nm_ip4_config_del_domain (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_return_if_fail (i < priv->domains->len);

	g_ptr_array_remove_index (priv->domains, i);
	_notify (self, PROP_DOMAINS);
}

guint
nm_ip4_config_get_num_domains (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->domains->len;
}

const char *
nm_ip4_config_get_domain (const NMIP4Config *self, guint i)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return g_ptr_array_index (priv->domains, i);
}

/*****************************************************************************/

void
nm_ip4_config_reset_searches (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->searches->len != 0) {
		g_ptr_array_set_size (priv->searches, 0);
		_notify (self, PROP_SEARCHES);
	}
}

void
nm_ip4_config_add_search (NMIP4Config *self, const char *new)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	char *search;
	size_t len;

	g_return_if_fail (new != NULL);
	g_return_if_fail (new[0] != '\0');

	search = g_strdup (new);

	/* Remove trailing dot as it has no effect */
	len = strlen (search);
	if (search[len - 1] == '.')
		search[len - 1] = 0;

	if (!search[0]) {
		g_free (search);
		return;
	}

	if (nm_utils_strv_find_first ((char **) priv->searches->pdata,
	                               priv->searches->len, search) >= 0) {
		g_free (search);
		return;
	}

	g_ptr_array_add (priv->searches, search);
	_notify (self, PROP_SEARCHES);
}

void
nm_ip4_config_del_search (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_return_if_fail (i < priv->searches->len);

	g_ptr_array_remove_index (priv->searches, i);
	_notify (self, PROP_SEARCHES);
}

guint
nm_ip4_config_get_num_searches (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->searches->len;
}

const char *
nm_ip4_config_get_search (const NMIP4Config *self, guint i)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return g_ptr_array_index (priv->searches, i);
}

/*****************************************************************************/

void
nm_ip4_config_reset_dns_options (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->dns_options->len != 0) {
		g_ptr_array_set_size (priv->dns_options, 0);
		_notify (self, PROP_DNS_OPTIONS);
	}
}

void
nm_ip4_config_add_dns_option (NMIP4Config *self, const char *new)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	int i;

	g_return_if_fail (new != NULL);
	g_return_if_fail (new[0] != '\0');

	for (i = 0; i < priv->dns_options->len; i++)
		if (!g_strcmp0 (g_ptr_array_index (priv->dns_options, i), new))
			return;

	g_ptr_array_add (priv->dns_options, g_strdup (new));
	_notify (self, PROP_DNS_OPTIONS);
}

void
nm_ip4_config_del_dns_option(NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_return_if_fail (i < priv->dns_options->len);

	g_ptr_array_remove_index (priv->dns_options, i);
	_notify (self, PROP_DNS_OPTIONS);
}

guint
nm_ip4_config_get_num_dns_options (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->dns_options->len;
}

const char *
nm_ip4_config_get_dns_option (const NMIP4Config *self, guint i)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return g_ptr_array_index (priv->dns_options, i);
}

/*****************************************************************************/

void
nm_ip4_config_set_dns_priority (NMIP4Config *self, gint priority)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priority != priv->dns_priority) {
		priv->dns_priority = priority;
		_notify (self, PROP_DNS_PRIORITY);
	}
}

gint
nm_ip4_config_get_dns_priority (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->dns_priority;
}

/*****************************************************************************/

void
nm_ip4_config_set_mss (NMIP4Config *self, guint32 mss)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	priv->mss = mss;
}

guint32
nm_ip4_config_get_mss (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->mss;
}

/*****************************************************************************/

void
nm_ip4_config_reset_nis_servers (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_array_set_size (priv->nis, 0);
}

void
nm_ip4_config_add_nis_server (NMIP4Config *self, guint32 nis)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	int i;

	for (i = 0; i < priv->nis->len; i++)
		if (nis == g_array_index (priv->nis, guint32, i))
			return;

	g_array_append_val (priv->nis, nis);
}

void
nm_ip4_config_del_nis_server (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_return_if_fail (i < priv->nis->len);

	g_array_remove_index (priv->nis, i);
}

guint
nm_ip4_config_get_num_nis_servers (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->nis->len;
}

guint32
nm_ip4_config_get_nis_server (const NMIP4Config *self, guint i)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return g_array_index (priv->nis, guint32, i);
}

void
nm_ip4_config_set_nis_domain (NMIP4Config *self, const char *domain)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_free (priv->nis_domain);
	priv->nis_domain = g_strdup (domain);
}

const char *
nm_ip4_config_get_nis_domain (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->nis_domain;
}

/*****************************************************************************/

void
nm_ip4_config_reset_wins (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (priv->wins->len != 0) {
		g_array_set_size (priv->wins, 0);
		_notify (self, PROP_WINS_SERVERS);
	}
}

void
nm_ip4_config_add_wins (NMIP4Config *self, guint32 wins)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	int i;

	g_return_if_fail (wins != 0);

	for (i = 0; i < priv->wins->len; i++)
		if (wins == g_array_index (priv->wins, guint32, i))
			return;

	g_array_append_val (priv->wins, wins);
	_notify (self, PROP_WINS_SERVERS);
}

void
nm_ip4_config_del_wins (NMIP4Config *self, guint i)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	g_return_if_fail (i < priv->wins->len);

	g_array_remove_index (priv->wins, i);
	_notify (self, PROP_WINS_SERVERS);
}

guint
nm_ip4_config_get_num_wins (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->wins->len;
}

guint32
nm_ip4_config_get_wins (const NMIP4Config *self, guint i)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return g_array_index (priv->wins, guint32, i);
}

/*****************************************************************************/

void
nm_ip4_config_set_mtu (NMIP4Config *self, guint32 mtu, NMIPConfigSource source)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	if (!mtu)
		source = NM_IP_CONFIG_SOURCE_UNKNOWN;

	priv->mtu = mtu;
	priv->mtu_source = source;
}

guint32
nm_ip4_config_get_mtu (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->mtu;
}

NMIPConfigSource
nm_ip4_config_get_mtu_source (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->mtu_source;
}

/*****************************************************************************/

void
nm_ip4_config_set_metered (NMIP4Config *self, gboolean metered)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	priv->metered = metered;
}

gboolean
nm_ip4_config_get_metered (const NMIP4Config *self)
{
	const NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	return priv->metered;
}

/*****************************************************************************/

static inline void
hash_u32 (GChecksum *sum, guint32 n)
{
	g_checksum_update (sum, (const guint8 *) &n, sizeof (n));
}

void
nm_ip4_config_hash (const NMIP4Config *self, GChecksum *sum, gboolean dns_only)
{
	guint i;
	const char *s;
	NMDedupMultiIter ipconf_iter;
	const NMPlatformIP4Address *address;
	const NMPlatformIP4Route *route;

	g_return_if_fail (self);
	g_return_if_fail (sum);

	if (!dns_only) {
		hash_u32 (sum, nm_ip4_config_has_gateway (self));
		hash_u32 (sum, nm_ip4_config_get_gateway (self));

		nm_ip_config_iter_ip4_address_for_each (&ipconf_iter, self, &address) {
			hash_u32 (sum, address->address);
			hash_u32 (sum, address->plen);
			hash_u32 (sum, address->peer_address & _nm_utils_ip4_prefix_to_netmask (address->plen));
		}

		nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, self, &route) {
			hash_u32 (sum, route->network);
			hash_u32 (sum, route->plen);
			hash_u32 (sum, route->gateway);
			hash_u32 (sum, route->metric);
		}

		for (i = 0; i < nm_ip4_config_get_num_nis_servers (self); i++)
			hash_u32 (sum, nm_ip4_config_get_nis_server (self, i));

		s = nm_ip4_config_get_nis_domain (self);
		if (s)
			g_checksum_update (sum, (const guint8 *) s, strlen (s));
	}

	for (i = 0; i < nm_ip4_config_get_num_nameservers (self); i++)
		hash_u32 (sum, nm_ip4_config_get_nameserver (self, i));

	for (i = 0; i < nm_ip4_config_get_num_wins (self); i++)
		hash_u32 (sum, nm_ip4_config_get_wins (self, i));

	for (i = 0; i < nm_ip4_config_get_num_domains (self); i++) {
		s = nm_ip4_config_get_domain (self, i);
		g_checksum_update (sum, (const guint8 *) s, strlen (s));
	}

	for (i = 0; i < nm_ip4_config_get_num_searches (self); i++) {
		s = nm_ip4_config_get_search (self, i);
		g_checksum_update (sum, (const guint8 *) s, strlen (s));
	}

	for (i = 0; i < nm_ip4_config_get_num_dns_options (self); i++) {
		s = nm_ip4_config_get_dns_option (self, i);
		g_checksum_update (sum, (const guint8 *) s, strlen (s));
	}
}

/**
 * nm_ip4_config_equal:
 * @a: first config to compare
 * @b: second config to compare
 *
 * Compares two #NMIP4Configs for basic equality.  This means that all
 * attributes must exist in the same order in both configs (addresses, routes,
 * domains, DNS servers, etc) but some attributes (address lifetimes, and address
 * and route sources) are ignored.
 *
 * Returns: %TRUE if the configurations are basically equal to each other,
 * %FALSE if not
 */
gboolean
nm_ip4_config_equal (const NMIP4Config *a, const NMIP4Config *b)
{
	GChecksum *a_checksum = g_checksum_new (G_CHECKSUM_SHA1);
	GChecksum *b_checksum = g_checksum_new (G_CHECKSUM_SHA1);
	gsize a_len = g_checksum_type_get_length (G_CHECKSUM_SHA1);
	gsize b_len = g_checksum_type_get_length (G_CHECKSUM_SHA1);
	guchar a_data[a_len], b_data[b_len];
	gboolean equal;

	if (a)
		nm_ip4_config_hash (a, a_checksum, FALSE);
	if (b)
		nm_ip4_config_hash (b, b_checksum, FALSE);

	g_checksum_get_digest (a_checksum, a_data, &a_len);
	g_checksum_get_digest (b_checksum, b_data, &b_len);

	g_assert (a_len == b_len);
	equal = !memcmp (a_data, b_data, a_len);

	g_checksum_free (a_checksum);
	g_checksum_free (b_checksum);

	return equal;
}

/*****************************************************************************/

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	NMIP4Config *self = NM_IP4_CONFIG (object);
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);
	const NMDedupMultiHeadEntry *head_entry;
	NMDedupMultiIter ipconf_iter;
	const NMPlatformIP4Route *route;
	GVariantBuilder builder_data, builder_legacy;

	switch (prop_id) {
	case PROP_IFINDEX:
		g_value_set_int (value, priv->ifindex);
		break;
	case PROP_ADDRESS_DATA:
	case PROP_ADDRESSES:
		nm_assert (!!priv->address_data_variant == !!priv->addresses_variant);

		if (priv->address_data_variant)
			goto out_addresses_cached;

		g_variant_builder_init (&builder_data, G_VARIANT_TYPE ("aa{sv}"));
		g_variant_builder_init (&builder_legacy, G_VARIANT_TYPE ("aau"));

		head_entry = nm_ip4_config_lookup_addresses (self);
		if (head_entry) {
			gs_free const NMPObject **addresses = NULL;
			guint naddr, i;

			addresses = (const NMPObject **) nm_dedup_multi_objs_to_array_head (head_entry, NULL, NULL, &naddr);
			nm_assert (addresses && naddr);

			g_qsort_with_data (addresses,
			                   naddr,
			                   sizeof (addresses[0]),
			                   _addresses_sort_cmp,
			                   NULL);

			/* Build address data variant */
			for (i = 0; i < naddr; i++) {
				GVariantBuilder addr_builder;
				const NMPlatformIP4Address *address = NMP_OBJECT_CAST_IP4_ADDRESS (addresses[i]);

				g_variant_builder_init (&addr_builder, G_VARIANT_TYPE ("a{sv}"));
				g_variant_builder_add (&addr_builder, "{sv}",
				                       "address",
				                       g_variant_new_string (nm_utils_inet4_ntop (address->address, NULL)));
				g_variant_builder_add (&addr_builder, "{sv}",
				                       "prefix",
				                       g_variant_new_uint32 (address->plen));
				if (address->peer_address != address->address) {
					g_variant_builder_add (&addr_builder, "{sv}",
					                       "peer",
					                       g_variant_new_string (nm_utils_inet4_ntop (address->peer_address, NULL)));
				}

				if (*address->label) {
					g_variant_builder_add (&addr_builder, "{sv}",
					                       "label",
					                       g_variant_new_string (address->label));
				}

				g_variant_builder_add (&builder_data, "a{sv}", &addr_builder);

				{
					const guint32 dbus_addr[3] = {
					    address->address,
					    address->plen,
					    i == 0 ? priv->gateway : 0,
					};

					g_variant_builder_add (&builder_legacy, "@au",
					                       g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32,
					                                                  dbus_addr, 3, sizeof (guint32)));
				}
			}
		}

		priv->address_data_variant = g_variant_ref_sink (g_variant_builder_end (&builder_data));
		priv->addresses_variant = g_variant_ref_sink (g_variant_builder_end (&builder_legacy));

out_addresses_cached:
		g_value_set_variant (value,
		                     prop_id == PROP_ADDRESS_DATA ?
		                     priv->address_data_variant :
		                     priv->addresses_variant);
		break;
	case PROP_ROUTE_DATA:
	case PROP_ROUTES:
		nm_assert (!!priv->route_data_variant == !!priv->routes_variant);

		if (priv->route_data_variant)
			goto out_routes_cached;

		g_variant_builder_init (&builder_data, G_VARIANT_TYPE ("aa{sv}"));
		g_variant_builder_init (&builder_legacy, G_VARIANT_TYPE ("aau"));

		nm_ip_config_iter_ip4_route_for_each (&ipconf_iter, self, &route) {
			GVariantBuilder route_builder;

			nm_assert (_route_valid (route));

			g_variant_builder_init (&route_builder, G_VARIANT_TYPE ("a{sv}"));
			g_variant_builder_add (&route_builder, "{sv}",
			                       "dest",
			                       g_variant_new_string (nm_utils_inet4_ntop (route->network, NULL)));
			g_variant_builder_add (&route_builder, "{sv}",
			                       "prefix",
			                       g_variant_new_uint32 (route->plen));
			if (route->gateway) {
				g_variant_builder_add (&route_builder, "{sv}",
				                       "next-hop",
				                       g_variant_new_string (nm_utils_inet4_ntop (route->gateway, NULL)));
			}
			g_variant_builder_add (&route_builder, "{sv}",
			                       "metric",
			                       g_variant_new_uint32 (route->metric));

			g_variant_builder_add (&builder_data, "a{sv}", &route_builder);

			/* legacy versions of nm_ip4_route_set_prefix() in libnm-util assert that the
			 * plen is positive. Skip the default routes not to break older clients. */
			if (!NM_PLATFORM_IP_ROUTE_IS_DEFAULT (route)) {
				const guint32 dbus_route[4] = {
				    route->network,
				    route->plen,
				    route->gateway,
				    route->metric,
				};

				g_variant_builder_add (&builder_legacy, "@au",
				                       g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32,
				                                                  dbus_route, 4, sizeof (guint32)));
			}
		}

		priv->route_data_variant = g_variant_ref_sink (g_variant_builder_end (&builder_data));
		priv->routes_variant = g_variant_ref_sink (g_variant_builder_end (&builder_legacy));

out_routes_cached:
		g_value_set_variant (value,
		                     prop_id == PROP_ROUTE_DATA ?
		                     priv->route_data_variant :
		                     priv->routes_variant);
		break;
	case PROP_GATEWAY:
		if (priv->has_gateway)
			g_value_set_string (value, nm_utils_inet4_ntop (priv->gateway, NULL));
		else
			g_value_set_string (value, NULL);
		break;
	case PROP_NAMESERVERS:
		g_value_take_variant (value,
		                      g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32,
		                                                 priv->nameservers->data,
		                                                 priv->nameservers->len,
		                                                 sizeof (guint32)));
		break;
	case PROP_DOMAINS:
		nm_utils_g_value_set_strv (value, priv->domains);
		break;
	case PROP_SEARCHES:
		nm_utils_g_value_set_strv (value, priv->searches);
		break;
	case PROP_DNS_OPTIONS:
		nm_utils_g_value_set_strv (value, priv->dns_options);
		break;
	case PROP_DNS_PRIORITY:
		g_value_set_int (value, priv->dns_priority);
		break;
	case PROP_WINS_SERVERS:
		g_value_take_variant (value,
		                      g_variant_new_fixed_array (G_VARIANT_TYPE_UINT32,
		                                                 priv->wins->data,
		                                                 priv->wins->len,
		                                                 sizeof (guint32)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_property (GObject *object,
              guint prop_id,
              const GValue *value,
              GParamSpec *pspec)
{
	NMIP4Config *self = NM_IP4_CONFIG (object);
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	switch (prop_id) {
	case PROP_MULTI_IDX:
		/* construct-only */
		priv->multi_idx = g_value_get_pointer (value);
		if (!priv->multi_idx)
			g_return_if_reached ();
		nm_dedup_multi_index_ref (priv->multi_idx);
		break;
	case PROP_IFINDEX:
		/* construct-only */
		priv->ifindex = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*****************************************************************************/

static void
nm_ip4_config_init (NMIP4Config *self)
{
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	nm_ip_config_dedup_multi_idx_type_init ((NMIPConfigDedupMultiIdxType *) &priv->idx_ip4_addresses,
	                                        NMP_OBJECT_TYPE_IP4_ADDRESS);
	nm_ip_config_dedup_multi_idx_type_init ((NMIPConfigDedupMultiIdxType *) &priv->idx_ip4_routes,
	                                        NMP_OBJECT_TYPE_IP4_ROUTE);

	priv->nameservers = g_array_new (FALSE, FALSE, sizeof (guint32));
	priv->domains = g_ptr_array_new_with_free_func (g_free);
	priv->searches = g_ptr_array_new_with_free_func (g_free);
	priv->dns_options = g_ptr_array_new_with_free_func (g_free);
	priv->nis = g_array_new (FALSE, TRUE, sizeof (guint32));
	priv->wins = g_array_new (FALSE, TRUE, sizeof (guint32));
	priv->route_metric = -1;
}

NMIP4Config *
nm_ip4_config_new (NMDedupMultiIndex *multi_idx, int ifindex)
{
	g_return_val_if_fail (ifindex >= -1, NULL);
	return (NMIP4Config *) g_object_new (NM_TYPE_IP4_CONFIG,
	                                     NM_IP4_CONFIG_MULTI_IDX, multi_idx,
	                                     NM_IP4_CONFIG_IFINDEX, ifindex,
	                                     NULL);
}

static void
finalize (GObject *object)
{
	NMIP4Config *self = NM_IP4_CONFIG (object);
	NMIP4ConfigPrivate *priv = NM_IP4_CONFIG_GET_PRIVATE (self);

	nm_dedup_multi_index_remove_idx (priv->multi_idx, &priv->idx_ip4_addresses);
	nm_dedup_multi_index_remove_idx (priv->multi_idx, &priv->idx_ip4_routes);

	nm_clear_g_variant (&priv->address_data_variant);
	nm_clear_g_variant (&priv->addresses_variant);
	nm_clear_g_variant (&priv->route_data_variant);
	nm_clear_g_variant (&priv->routes_variant);

	g_array_unref (priv->nameservers);
	g_ptr_array_unref (priv->domains);
	g_ptr_array_unref (priv->searches);
	g_ptr_array_unref (priv->dns_options);
	g_array_unref (priv->nis);
	g_free (priv->nis_domain);
	g_array_unref (priv->wins);

	G_OBJECT_CLASS (nm_ip4_config_parent_class)->finalize (object);

	nm_dedup_multi_index_unref (priv->multi_idx);
}

static void
nm_ip4_config_class_init (NMIP4ConfigClass *config_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (config_class);
	NMExportedObjectClass *exported_object_class = NM_EXPORTED_OBJECT_CLASS (config_class);

	exported_object_class->export_path = NM_EXPORT_PATH_NUMBERED (NM_DBUS_PATH"/IP4Config");

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->finalize = finalize;

	obj_properties[PROP_MULTI_IDX] =
	    g_param_spec_pointer (NM_IP4_CONFIG_MULTI_IDX, "", "",
	                            G_PARAM_WRITABLE
	                          | G_PARAM_CONSTRUCT_ONLY
	                          | G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_IFINDEX] =
	    g_param_spec_int (NM_IP4_CONFIG_IFINDEX, "", "",
	                      -1, G_MAXINT, -1,
	                      G_PARAM_READWRITE |
	                      G_PARAM_CONSTRUCT_ONLY |
	                      G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_ADDRESS_DATA] =
	    g_param_spec_variant (NM_IP4_CONFIG_ADDRESS_DATA, "", "",
	                          G_VARIANT_TYPE ("aa{sv}"),
	                          NULL,
	                          G_PARAM_READABLE |
	                          G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_ADDRESSES] =
	    g_param_spec_variant (NM_IP4_CONFIG_ADDRESSES, "", "",
	                          G_VARIANT_TYPE ("aau"),
	                          NULL,
	                          G_PARAM_READABLE |
	                          G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_ROUTE_DATA] =
	    g_param_spec_variant (NM_IP4_CONFIG_ROUTE_DATA, "", "",
	                          G_VARIANT_TYPE ("aa{sv}"),
	                          NULL,
	                          G_PARAM_READABLE |
	                          G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_ROUTES] =
	    g_param_spec_variant (NM_IP4_CONFIG_ROUTES, "", "",
	                          G_VARIANT_TYPE ("aau"),
	                          NULL,
	                          G_PARAM_READABLE |
	                          G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_GATEWAY] =
	    g_param_spec_string (NM_IP4_CONFIG_GATEWAY, "", "",
	                         NULL,
	                         G_PARAM_READABLE |
	                         G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_NAMESERVERS] =
	    g_param_spec_variant (NM_IP4_CONFIG_NAMESERVERS, "", "",
	                          G_VARIANT_TYPE ("au"),
	                          NULL,
	                          G_PARAM_READABLE |
	                          G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_DOMAINS] =
	    g_param_spec_boxed (NM_IP4_CONFIG_DOMAINS, "", "",
	                        G_TYPE_STRV,
	                        G_PARAM_READABLE |
	                        G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_SEARCHES] =
	    g_param_spec_boxed (NM_IP4_CONFIG_SEARCHES, "", "",
	                        G_TYPE_STRV,
	                        G_PARAM_READABLE |
	                        G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_DNS_OPTIONS] =
	     g_param_spec_boxed (NM_IP4_CONFIG_DNS_OPTIONS, "", "",
	                         G_TYPE_STRV,
	                         G_PARAM_READABLE |
	                         G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_DNS_PRIORITY] =
	     g_param_spec_int (NM_IP4_CONFIG_DNS_PRIORITY, "", "",
	                       G_MININT32, G_MAXINT32, 0,
	                       G_PARAM_READABLE |
	                       G_PARAM_STATIC_STRINGS);
	obj_properties[PROP_WINS_SERVERS] =
	    g_param_spec_variant (NM_IP4_CONFIG_WINS_SERVERS, "", "",
	                          G_VARIANT_TYPE ("au"),
	                          NULL,
	                          G_PARAM_READABLE |
	                          G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, _PROPERTY_ENUMS_LAST, obj_properties);

	nm_exported_object_class_add_interface (NM_EXPORTED_OBJECT_CLASS (config_class),
	                                        NMDBUS_TYPE_IP4_CONFIG_SKELETON,
	                                        NULL);
}
