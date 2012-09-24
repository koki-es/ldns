/*
 * special zone file structures and functions for better dnssec handling
 */

#include <ldns/config.h>

#include <ldns/ldns.h>

ldns_dnssec_rrs *
ldns_dnssec_rrs_new()
{
	ldns_dnssec_rrs *new_rrs;
	new_rrs = LDNS_MALLOC(ldns_dnssec_rrs);
        if(!new_rrs) return NULL;
	new_rrs->rr = NULL;
	new_rrs->next = NULL;
	return new_rrs;
}

INLINE void
ldns_dnssec_rrs_free_internal(ldns_dnssec_rrs *rrs, int deep)
{
	ldns_dnssec_rrs *next;
	while (rrs) {
		next = rrs->next;
		if (deep) {
			ldns_rr_free(rrs->rr);
		}
		LDNS_FREE(rrs);
		rrs = next;
	}
}

void
ldns_dnssec_rrs_free(ldns_dnssec_rrs *rrs)
{
	ldns_dnssec_rrs_free_internal(rrs, 0);
}

void
ldns_dnssec_rrs_deep_free(ldns_dnssec_rrs *rrs)
{
	ldns_dnssec_rrs_free_internal(rrs, 1);
}

ldns_status
ldns_dnssec_rrs_add_rr(ldns_dnssec_rrs *rrs, ldns_rr *rr)
{
	int cmp;
	ldns_dnssec_rrs *new_rrs;
	if (!rrs || !rr) {
		return LDNS_STATUS_ERR;
	}

	/* this could be done more efficiently; name and type should already
	   be equal */
	cmp = ldns_rr_compare(rrs->rr,
					  rr);
	/* should we error on equal? */
	if (cmp <= 0) {
		if (rrs->next) {
			return ldns_dnssec_rrs_add_rr(rrs->next, rr);
		} else {
			new_rrs = ldns_dnssec_rrs_new();
			new_rrs->rr = rr;
			rrs->next = new_rrs;
		}
	} else if (cmp > 0) {
		/* put the current old rr in the new next, put the new
		   rr in the current container */
		new_rrs = ldns_dnssec_rrs_new();
		new_rrs->rr = rrs->rr;
		new_rrs->next = rrs->next;
		rrs->rr = rr;
		rrs->next = new_rrs;
	}
	return LDNS_STATUS_OK;
}

void
ldns_dnssec_rrs_print_fmt(FILE *out, const ldns_output_format *fmt,
	       ldns_dnssec_rrs *rrs)
{
	if (!rrs) {
		if ((fmt->flags & LDNS_COMMENT_LAYOUT))
			fprintf(out, "; <void>");
	} else {
		if (rrs->rr) {
			ldns_rr_print_fmt(out, fmt, rrs->rr);
		}
		if (rrs->next) {
			ldns_dnssec_rrs_print_fmt(out, fmt, rrs->next);
		}
	}
}

void
ldns_dnssec_rrs_print(FILE *out, ldns_dnssec_rrs *rrs)
{
	ldns_dnssec_rrs_print_fmt(out, ldns_output_format_default, rrs);
}


ldns_dnssec_rrsets *
ldns_dnssec_rrsets_new()
{
	ldns_dnssec_rrsets *new_rrsets;
	new_rrsets = LDNS_MALLOC(ldns_dnssec_rrsets);
        if(!new_rrsets) return NULL;
	new_rrsets->rrs = NULL;
	new_rrsets->type = 0;
	new_rrsets->signatures = NULL;
	new_rrsets->next = NULL;
	return new_rrsets;
}

INLINE void
ldns_dnssec_rrsets_free_internal(ldns_dnssec_rrsets *rrsets, int deep)
{
	if (rrsets) {
		if (rrsets->rrs) {
			ldns_dnssec_rrs_free_internal(rrsets->rrs, deep);
		}
		if (rrsets->next) {
			ldns_dnssec_rrsets_free_internal(rrsets->next, deep);
		}
		if (rrsets->signatures) {
			ldns_dnssec_rrs_free_internal(rrsets->signatures, deep);
		}
		LDNS_FREE(rrsets);
	}
}

void
ldns_dnssec_rrsets_free(ldns_dnssec_rrsets *rrsets)
{
	ldns_dnssec_rrsets_free_internal(rrsets, 0);
}

void
ldns_dnssec_rrsets_deep_free(ldns_dnssec_rrsets *rrsets)
{
	ldns_dnssec_rrsets_free_internal(rrsets, 1);
}

ldns_rr_type
ldns_dnssec_rrsets_type(ldns_dnssec_rrsets *rrsets)
{
	if (rrsets) {
		return rrsets->type;
	} else {
		return 0;
	}
}

ldns_status
ldns_dnssec_rrsets_set_type(ldns_dnssec_rrsets *rrsets,
					   ldns_rr_type type)
{
	if (rrsets) {
		rrsets->type = type;
		return LDNS_STATUS_OK;
	}
	return LDNS_STATUS_ERR;
}

ldns_dnssec_rrsets *
ldns_dnssec_rrsets_new_frm_rr(ldns_rr *rr)
{
	ldns_dnssec_rrsets *new_rrsets;
	ldns_rr_type rr_type;
	bool rrsig;

	new_rrsets = ldns_dnssec_rrsets_new();
	rr_type = ldns_rr_get_type(rr);
	if (rr_type == LDNS_RR_TYPE_RRSIG) {
		rrsig = true;
		rr_type = ldns_rdf2rr_type(ldns_rr_rrsig_typecovered(rr));
	} else {
		rrsig = false;
	}
	if (!rrsig) {
		new_rrsets->rrs = ldns_dnssec_rrs_new();
		new_rrsets->rrs->rr = rr;
	} else {
		new_rrsets->signatures = ldns_dnssec_rrs_new();
		new_rrsets->signatures->rr = rr;
	}
	new_rrsets->type = rr_type;
	return new_rrsets;
}

ldns_status
ldns_dnssec_rrsets_add_rr(ldns_dnssec_rrsets *rrsets, ldns_rr *rr)
{
	ldns_dnssec_rrsets *new_rrsets;
	ldns_rr_type rr_type;
	bool rrsig = false;
	ldns_status result = LDNS_STATUS_OK;

	if (!rrsets || !rr) {
		return LDNS_STATUS_ERR;
	}

	rr_type = ldns_rr_get_type(rr);

	if (rr_type == LDNS_RR_TYPE_RRSIG) {
		rrsig = true;
		rr_type = ldns_rdf2rr_type(ldns_rr_rrsig_typecovered(rr));
	}

	if (!rrsets->rrs && rrsets->type == 0 && !rrsets->signatures) {
		if (!rrsig) {
			rrsets->rrs = ldns_dnssec_rrs_new();
			rrsets->rrs->rr = rr;
			rrsets->type = rr_type;
		} else {
			rrsets->signatures = ldns_dnssec_rrs_new();
			rrsets->signatures->rr = rr;
			rrsets->type = rr_type;
		}
		return LDNS_STATUS_OK;
	}

	if (rr_type > ldns_dnssec_rrsets_type(rrsets)) {
		if (rrsets->next) {
			result = ldns_dnssec_rrsets_add_rr(rrsets->next, rr);
		} else {
			new_rrsets = ldns_dnssec_rrsets_new_frm_rr(rr);
			rrsets->next = new_rrsets;
		}
	} else if (rr_type < ldns_dnssec_rrsets_type(rrsets)) {
		/* move the current one into the new next, 
		   replace field of current with data from new rr */
		new_rrsets = ldns_dnssec_rrsets_new();
		new_rrsets->rrs = rrsets->rrs;
		new_rrsets->type = rrsets->type;
		new_rrsets->signatures = rrsets->signatures;
		new_rrsets->next = rrsets->next;
		if (!rrsig) {
			rrsets->rrs = ldns_dnssec_rrs_new();
			rrsets->rrs->rr = rr;
			rrsets->signatures = NULL;
		} else {
			rrsets->rrs = NULL;
			rrsets->signatures = ldns_dnssec_rrs_new();
			rrsets->signatures->rr = rr;
		}
		rrsets->type = rr_type;
		rrsets->next = new_rrsets;
	} else {
		/* equal, add to current rrsets */
		if (rrsig) {
			if (rrsets->signatures) {
				result = ldns_dnssec_rrs_add_rr(rrsets->signatures, rr);
			} else {
				rrsets->signatures = ldns_dnssec_rrs_new();
				rrsets->signatures->rr = rr;
			}
		} else {
			if (rrsets->rrs) {
				result = ldns_dnssec_rrs_add_rr(rrsets->rrs, rr);
			} else {
				rrsets->rrs = ldns_dnssec_rrs_new();
				rrsets->rrs->rr = rr;
			}
		}
	}

	return result;
}

void
ldns_dnssec_rrsets_print_soa_fmt(FILE *out, const ldns_output_format *fmt,
		ldns_dnssec_rrsets *rrsets,
		bool follow,
		bool show_soa)
{
	if (!rrsets) {
		if ((fmt->flags & LDNS_COMMENT_LAYOUT))
			fprintf(out, "; <void>\n");
	} else {
		if (rrsets->rrs &&
		    (show_soa ||
			ldns_rr_get_type(rrsets->rrs->rr) != LDNS_RR_TYPE_SOA
		    )
		   ) {
			ldns_dnssec_rrs_print_fmt(out, fmt, rrsets->rrs);
			if (rrsets->signatures) {
				ldns_dnssec_rrs_print_fmt(out, fmt, 
						rrsets->signatures);
			}
		}
		if (follow && rrsets->next) {
			ldns_dnssec_rrsets_print_soa_fmt(out, fmt, 
					rrsets->next, follow, show_soa);
		}
	}
}

void
ldns_dnssec_rrsets_print_soa(FILE *out,
		ldns_dnssec_rrsets *rrsets,
		bool follow,
		bool show_soa)
{
	ldns_dnssec_rrsets_print_soa_fmt(out, ldns_output_format_default,
		       	rrsets, follow, show_soa);
}


void
ldns_dnssec_rrsets_print_fmt(FILE *out, const ldns_output_format *fmt,
		ldns_dnssec_rrsets *rrsets, 
		bool follow)
{
	ldns_dnssec_rrsets_print_soa_fmt(out, fmt, rrsets, follow, true);
}

void
ldns_dnssec_rrsets_print(FILE *out, ldns_dnssec_rrsets *rrsets, bool follow)
{
	ldns_dnssec_rrsets_print_fmt(out, ldns_output_format_default, 
			rrsets, follow);
}

ldns_dnssec_name *
ldns_dnssec_name_new()
{
	ldns_dnssec_name *new_name;

	new_name = LDNS_CALLOC(ldns_dnssec_name, 1);
	if (!new_name) {
		return NULL;
	}
	/*
	 * not needed anymore because CALLOC initalizes everything to zero.

	new_name->name = NULL;
	new_name->rrsets = NULL;
	new_name->name_alloced = false;
	new_name->nsec = NULL;
	new_name->nsec_signatures = NULL;

	new_name->is_glue = false;
	new_name->hashed_name = NULL;

	 */
	return new_name;
}

ldns_dnssec_name *
ldns_dnssec_name_new_frm_rr(ldns_rr *rr)
{
	ldns_dnssec_name *new_name = ldns_dnssec_name_new();

	new_name->name = ldns_rr_owner(rr);
	if(ldns_dnssec_name_add_rr(new_name, rr) != LDNS_STATUS_OK) {
		ldns_dnssec_name_free(new_name);
		return NULL;
	}

	return new_name;
}

INLINE void
ldns_dnssec_name_free_internal(ldns_dnssec_name *name,
                               int deep)
{
	if (name) {
		if (name->name_alloced) {
			ldns_rdf_deep_free(name->name);
		}
		if (name->rrsets) {
			ldns_dnssec_rrsets_free_internal(name->rrsets, deep);
		}
		if (name->nsec && deep) {
			ldns_rr_free(name->nsec);
		}
		if (name->nsec_signatures) {
			ldns_dnssec_rrs_free_internal(name->nsec_signatures, deep);
		}
		if (name->hashed_name) {
			if (deep) {
				ldns_rdf_deep_free(name->hashed_name);
			}
		}
		LDNS_FREE(name);
	}
}

void
ldns_dnssec_name_free(ldns_dnssec_name *name)
{
  ldns_dnssec_name_free_internal(name, 0);
}

void
ldns_dnssec_name_deep_free(ldns_dnssec_name *name)
{
  ldns_dnssec_name_free_internal(name, 1);
}

ldns_rdf *
ldns_dnssec_name_name(ldns_dnssec_name *name)
{
	if (name) {
		return name->name;
	}
	return NULL;
}

bool
ldns_dnssec_name_is_glue(ldns_dnssec_name *name)
{
	if (name) {
		return name->is_glue;
	}
	return false;
}

void
ldns_dnssec_name_set_name(ldns_dnssec_name *rrset,
					 ldns_rdf *dname)
{
	if (rrset && dname) {
		rrset->name = dname;
	}
}

ldns_rr *
ldns_dnssec_name_nsec(ldns_dnssec_name *rrset)
{
	if (rrset) {
		return rrset->nsec;
	}
	return NULL;
}

void
ldns_dnssec_name_set_nsec(ldns_dnssec_name *rrset, ldns_rr *nsec)
{
	if (rrset && nsec) {
		rrset->nsec = nsec;
	}
}

int
ldns_dnssec_name_cmp(const void *a, const void *b)
{
	ldns_dnssec_name *na = (ldns_dnssec_name *) a;
	ldns_dnssec_name *nb = (ldns_dnssec_name *) b;

	if (na && nb) {
		return ldns_dname_compare(ldns_dnssec_name_name(na),
							 ldns_dnssec_name_name(nb));
	} else if (na) {
		return 1;
	} else if (nb) {
		return -1;
	} else {
		return 0;
	}
}

ldns_status
ldns_dnssec_name_add_rr(ldns_dnssec_name *name,
				    ldns_rr *rr)
{
	ldns_status result = LDNS_STATUS_OK;
	ldns_rdf *name_name;
	bool hashed_name = false;
	ldns_rr_type rr_type;
	ldns_rr_type typecovered = 0;

	/* special handling for NSEC3 and NSECX covering RRSIGS */

	if (!name || !rr) {
		return LDNS_STATUS_ERR;
	}

	rr_type = ldns_rr_get_type(rr);

	if (rr_type == LDNS_RR_TYPE_RRSIG) {
		typecovered = ldns_rdf2rr_type(ldns_rr_rrsig_typecovered(rr));
	}

#ifdef HAVE_SSL
	if (rr_type == LDNS_RR_TYPE_NSEC3 ||
	    typecovered == LDNS_RR_TYPE_NSEC3) {
		name_name = ldns_nsec3_hash_name_frm_nsec3(rr,
										   ldns_dnssec_name_name(name));
		hashed_name = true;
	} else {
		name_name = ldns_dnssec_name_name(name);
	}
#else
	name_name = ldns_dnssec_name_name(name);
#endif /* HAVE_SSL */

	if (rr_type == LDNS_RR_TYPE_NSEC ||
	    rr_type == LDNS_RR_TYPE_NSEC3) {
		/* XX check if is already set (and error?) */
		name->nsec = rr;
	} else if (typecovered == LDNS_RR_TYPE_NSEC ||
			 typecovered == LDNS_RR_TYPE_NSEC3) {
		if (name->nsec_signatures) {
			result = ldns_dnssec_rrs_add_rr(name->nsec_signatures, rr);
		} else {
			name->nsec_signatures = ldns_dnssec_rrs_new();
			name->nsec_signatures->rr = rr;
		}
	} else {
		/* it's a 'normal' RR, add it to the right rrset */
		if (name->rrsets) {
			result = ldns_dnssec_rrsets_add_rr(name->rrsets, rr);
		} else {
			name->rrsets = ldns_dnssec_rrsets_new();
			result = ldns_dnssec_rrsets_add_rr(name->rrsets, rr);
		}
	}

	if (hashed_name) {
		ldns_rdf_deep_free(name_name);
	}

	return result;
}

ldns_dnssec_rrsets *
ldns_dnssec_name_find_rrset(ldns_dnssec_name *name,
					   ldns_rr_type type) {
	ldns_dnssec_rrsets *result;

	result = name->rrsets;
	while (result) {
		if (result->type == type) {
			return result;
		} else {
			result = result->next;
		}
	}
	return NULL;
}

ldns_dnssec_rrsets *
ldns_dnssec_zone_find_rrset(ldns_dnssec_zone *zone,
					   ldns_rdf *dname,
					   ldns_rr_type type)
{
	ldns_rbnode_t *node;

	if (!zone || !dname) {
		return NULL;
	}

	node = ldns_rbtree_search(zone->names, dname);
	if (node) {
		return ldns_dnssec_name_find_rrset((ldns_dnssec_name *)node->data,
									type);
	} else {
		return NULL;
	}
}

void
ldns_dnssec_name_print_soa_fmt(FILE *out, const ldns_output_format *fmt,
		ldns_dnssec_name *name, 
		bool show_soa)
{
	if (name) {
		if(name->rrsets) {
			ldns_dnssec_rrsets_print_soa_fmt(out, fmt, 
					name->rrsets, true, show_soa);
		} else if ((fmt->flags & LDNS_COMMENT_LAYOUT)) {
			fprintf(out, ";; Empty nonterminal: ");
			ldns_rdf_print(out, name->name);
			fprintf(out, "\n");
		}
		if(name->nsec) {
			ldns_rr_print_fmt(out, fmt, name->nsec);
		}
		if (name->nsec_signatures) {
			ldns_dnssec_rrs_print_fmt(out, fmt, 
					name->nsec_signatures);
		}
	} else if ((fmt->flags & LDNS_COMMENT_LAYOUT)) {
		fprintf(out, "; <void>\n");
	}
}

void
ldns_dnssec_name_print_soa(FILE *out, ldns_dnssec_name *name, bool show_soa)
{
	ldns_dnssec_name_print_soa_fmt(out, ldns_output_format_default,
		       name, show_soa);
}

void
ldns_dnssec_name_print_fmt(FILE *out, const ldns_output_format *fmt,
		ldns_dnssec_name *name)
{
	ldns_dnssec_name_print_soa_fmt(out, fmt, name, true);
}

void
ldns_dnssec_name_print(FILE *out, ldns_dnssec_name *name)
{
	ldns_dnssec_name_print_fmt(out, ldns_output_format_default, name);
}


ldns_dnssec_zone *
ldns_dnssec_zone_new()
{
	ldns_dnssec_zone *zone = LDNS_MALLOC(ldns_dnssec_zone);
        if(!zone) return NULL;
	zone->soa = NULL;
	zone->names = NULL;

	return zone;
}

static bool
rr_is_rrsig_covering(ldns_rr* rr, ldns_rr_type t)
{
	return     ldns_rr_get_type(rr) == LDNS_RR_TYPE_RRSIG
		&& ldns_rdf2rr_type(ldns_rr_rrsig_typecovered(rr)) == t;
}

/* When the zone is first read into an list and then inserted into an
 * ldns_dnssec_zone (rbtree) the nodes of the rbtree are allocated close (next)
 * to each other. Because ldns-verify-zone (the only program that uses this
 * function) uses the rbtree mostly for sequentual walking, this results
 * in a speed increase (of 15% on linux) because we have less CPU-cache misses.
 */
#define FASTER_DNSSEC_ZONE_NEW_FRM_FP 1 /* Because of L2 cache efficiency */

ldns_status
ldns_dnssec_zone_new_frm_fp_l(ldns_dnssec_zone** z, FILE* fp, ldns_rdf* origin,
	       	uint32_t ttl, ldns_rr_class ATTR_UNUSED(c), int* line_nr)
{
	ldns_rr* cur_rr;
	size_t i;

	ldns_rdf *my_origin = NULL;
	ldns_rdf *my_prev = NULL;

	ldns_dnssec_zone *newzone = ldns_dnssec_zone_new();
	/* when reading NSEC3s, there is a chance that we encounter nsecs
	   for empty nonterminals, whose nonterminals we cannot derive yet
	   because the needed information is to be read later. in that case
	   we keep a list of those nsec3's and retry to add them later */
	ldns_rr_list* todo_nsec3s = ldns_rr_list_new();
	ldns_rr_list* todo_nsec3_rrsigs = ldns_rr_list_new();

	ldns_status status = LDNS_STATUS_MEM_ERR;

#ifdef FASTER_DNSSEC_ZONE_NEW_FRM_FP
	ldns_zone* zone = NULL;
	if (ldns_zone_new_frm_fp_l(&zone, fp, origin,ttl, c, line_nr)
			!= LDNS_STATUS_OK) goto error;
#else
	uint32_t  my_ttl = ttl;
#endif

	if (!newzone || !todo_nsec3s || !todo_nsec3_rrsigs ) goto error;

	if (origin) {
		if (!(my_origin = ldns_rdf_clone(origin))) goto error;
		if (!(my_prev   = ldns_rdf_clone(origin))) goto error;
	}

#ifdef FASTER_DNSSEC_ZONE_NEW_FRM_FP
	if (ldns_dnssec_zone_add_rr(newzone, ldns_zone_soa(zone))
			!= LDNS_STATUS_OK) goto error;

	for (i = 0; i < ldns_rr_list_rr_count(ldns_zone_rrs(zone)); i++) {
		cur_rr = ldns_rr_list_rr(ldns_zone_rrs(zone), i);
		status = LDNS_STATUS_OK;
#else
	while (!feof(fp)) {
		status = ldns_rr_new_frm_fp_l(&cur_rr, fp, &my_ttl, &my_origin,
				&my_prev, line_nr);

#endif
		switch (status) {
		case LDNS_STATUS_OK:

			status = ldns_dnssec_zone_add_rr(newzone, cur_rr);
			if (status ==
				LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND) {

				if (rr_is_rrsig_covering(cur_rr,
							LDNS_RR_TYPE_NSEC3)){
					ldns_rr_list_push_rr(todo_nsec3_rrsigs,
							cur_rr);
				} else {
					ldns_rr_list_push_rr(todo_nsec3s,
						       	cur_rr);
				}
			} else if (status != LDNS_STATUS_OK)
				goto error;

			break;


		case LDNS_STATUS_SYNTAX_EMPTY:	/* empty line was seen */
		case LDNS_STATUS_SYNTAX_TTL:	/* the ttl was set*/
		case LDNS_STATUS_SYNTAX_ORIGIN:	/* the origin was set*/
			status = LDNS_STATUS_OK;
			break;

		case LDNS_STATUS_SYNTAX_INCLUDE:/* $include not implemented */
			status =  LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL;
			break;

		default:
			goto error;
		}
	}

	if (ldns_rr_list_rr_count(todo_nsec3s) > 0) {
		(void) ldns_dnssec_zone_add_empty_nonterminals(newzone);
		for (i = 0; status == LDNS_STATUS_OK && 
				i < ldns_rr_list_rr_count(todo_nsec3s); i++) {
			cur_rr = ldns_rr_list_rr(todo_nsec3s, i);
			status = ldns_dnssec_zone_add_rr(newzone, cur_rr);
		}
		for (i = 0; status == LDNS_STATUS_OK &&
				i < ldns_rr_list_rr_count(todo_nsec3_rrsigs);
			       	i++){
			cur_rr = ldns_rr_list_rr(todo_nsec3_rrsigs, i);
			status = ldns_dnssec_zone_add_rr(newzone, cur_rr);
		}
	} else if (ldns_rr_list_rr_count(todo_nsec3_rrsigs) > 0) {
		for (i = 0; status == LDNS_STATUS_OK &&
				i < ldns_rr_list_rr_count(todo_nsec3_rrsigs);
				i++){
			cur_rr = ldns_rr_list_rr(todo_nsec3_rrsigs, i);
			status = ldns_dnssec_zone_add_rr(newzone, cur_rr);
		}
	}

	if (z) {
		*z = newzone;
		newzone = NULL;
	} else {
		ldns_dnssec_zone_free(newzone);
	}

error:
#ifdef FASTER_DNSSEC_ZONE_NEW_FRM_FP
	if (zone) {
		ldns_zone_free(zone);
	}
#endif
	ldns_rr_list_free(todo_nsec3_rrsigs);
	ldns_rr_list_free(todo_nsec3s);

	if (my_origin) {
		ldns_rdf_deep_free(my_origin);
	}
	if (my_prev) {
		ldns_rdf_deep_free(my_prev);
	}
	if (newzone) {
		ldns_dnssec_zone_free(newzone);
	}
	return status;
}

ldns_status
ldns_dnssec_zone_new_frm_fp(ldns_dnssec_zone** z, FILE* fp, ldns_rdf* origin,
		uint32_t ttl, ldns_rr_class ATTR_UNUSED(c))
{
	return ldns_dnssec_zone_new_frm_fp_l(z, fp, origin, ttl, c, NULL);
}

void
ldns_dnssec_name_node_free(ldns_rbnode_t *node, void *arg) {
	(void) arg;
	ldns_dnssec_name_free((ldns_dnssec_name *)node->data);
	LDNS_FREE(node);
}

void
ldns_dnssec_name_node_deep_free(ldns_rbnode_t *node, void *arg) {
	(void) arg;
	ldns_dnssec_name_deep_free((ldns_dnssec_name *)node->data);
	LDNS_FREE(node);
}

void
ldns_dnssec_zone_free(ldns_dnssec_zone *zone)
{
	if (zone) {
		if (zone->names) {
			/* destroy all name structures within the tree */
			ldns_traverse_postorder(zone->names,
						    ldns_dnssec_name_node_free,
						    NULL);
			LDNS_FREE(zone->names);
		}
		LDNS_FREE(zone);
	}
}

void
ldns_dnssec_zone_deep_free(ldns_dnssec_zone *zone)
{
	if (zone) {
		if (zone->names) {
			/* destroy all name structures within the tree */
			ldns_traverse_postorder(zone->names,
						    ldns_dnssec_name_node_deep_free,
						    NULL);
			LDNS_FREE(zone->names);
		}
		LDNS_FREE(zone);
	}
}

/* use for dname comparison in tree */
int
ldns_dname_compare_v(const void *a, const void *b) {
	return ldns_dname_compare((ldns_rdf *)a, (ldns_rdf *)b);
}

ldns_rbnode_t *
ldns_dnssec_zone_find_nsec3_original(ldns_dnssec_zone *zone,
                                     ldns_rr *rr) {
	ldns_rbnode_t *current_node = ldns_rbtree_first(zone->names);
	ldns_dnssec_name *current_name;
	ldns_rdf *hashed_name;

	hashed_name = ldns_dname_label(ldns_rr_owner(rr), 0);

	while (current_node != LDNS_RBTREE_NULL) {
		current_name = (ldns_dnssec_name *) current_node->data;
		if (!current_name->hashed_name) {
			current_name->hashed_name =
				ldns_nsec3_hash_name_frm_nsec3(rr, current_name->name);
		}
		if (ldns_dname_compare(hashed_name,
						   current_name->hashed_name)
		    == 0) {
			ldns_rdf_deep_free(hashed_name);
			return current_node;
		}
		current_node = ldns_rbtree_next(current_node);
	}
	ldns_rdf_deep_free(hashed_name);
	return NULL;
}

ldns_status
ldns_dnssec_zone_add_rr(ldns_dnssec_zone *zone, ldns_rr *rr)
{
	ldns_status result = LDNS_STATUS_OK;
	ldns_dnssec_name *cur_name;
	ldns_rbnode_t *cur_node;
	ldns_rr_type type_covered = 0;

	if (!zone || !rr) {
		return LDNS_STATUS_ERR;
	}

	if (!zone->names) {
		zone->names = ldns_rbtree_create(ldns_dname_compare_v);
                if(!zone->names) return LDNS_STATUS_MEM_ERR;
	}

	/* we need the original of the hashed name if this is
	   an NSEC3, or an RRSIG that covers an NSEC3 */
	if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_RRSIG) {
		type_covered = ldns_rdf2rr_type(ldns_rr_rrsig_typecovered(rr));
	}
	if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_NSEC3 ||
	    type_covered == LDNS_RR_TYPE_NSEC3) {
		cur_node = ldns_dnssec_zone_find_nsec3_original(zone,
					 						   rr);
		if (!cur_node) {
			return LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND;
		}
	} else {
		cur_node = ldns_rbtree_search(zone->names, ldns_rr_owner(rr));
	}

	if (!cur_node) {
		/* add */
		cur_name = ldns_dnssec_name_new_frm_rr(rr);
                if(!cur_name) return LDNS_STATUS_MEM_ERR;
		cur_node = LDNS_MALLOC(ldns_rbnode_t);
                if(!cur_node) {
                        ldns_dnssec_name_free(cur_name);
                        return LDNS_STATUS_MEM_ERR;
                }
		cur_node->key = ldns_rr_owner(rr);
		cur_node->data = cur_name;
		(void)ldns_rbtree_insert(zone->names, cur_node);
	} else {
		cur_name = (ldns_dnssec_name *) cur_node->data;
		result = ldns_dnssec_name_add_rr(cur_name, rr);
	}

	if (result != LDNS_STATUS_OK) {
		fprintf(stderr, "error adding rr: ");
		ldns_rr_print(stderr, rr);
	}

	/*TODO ldns_dnssec_name_print_names(stdout, zone->names, 0);*/
	if (ldns_rr_get_type(rr) == LDNS_RR_TYPE_SOA) {
		zone->soa = cur_name;
	}

	return result;
}

void
ldns_dnssec_zone_names_print_fmt(FILE *out, const ldns_output_format *fmt,
		ldns_rbtree_t *tree, 
		bool print_soa)
{
	ldns_rbnode_t *node;
	ldns_dnssec_name *name;

	node = ldns_rbtree_first(tree);
	while (node != LDNS_RBTREE_NULL) {
		name = (ldns_dnssec_name *) node->data;
		ldns_dnssec_name_print_soa_fmt(out, fmt, name, print_soa);
		if ((fmt->flags & LDNS_COMMENT_LAYOUT))
			fprintf(out, ";\n");
		node = ldns_rbtree_next(node);
	}
}

void
ldns_dnssec_zone_names_print(FILE *out, ldns_rbtree_t *tree, bool print_soa)
{
	ldns_dnssec_zone_names_print_fmt(out, ldns_output_format_default,
		       tree, print_soa);
}

void
ldns_dnssec_zone_print_fmt(FILE *out, const ldns_output_format *fmt,
	       ldns_dnssec_zone *zone)
{
	if (zone) {
		if (zone->soa) {
			if ((fmt->flags & LDNS_COMMENT_LAYOUT)) {
				fprintf(out, ";; Zone: ");
				ldns_rdf_print(out, ldns_dnssec_name_name(
							zone->soa));
				fprintf(out, "\n;\n");
			}
			ldns_dnssec_rrsets_print_fmt(out, fmt,
					ldns_dnssec_name_find_rrset(
						zone->soa, 
						LDNS_RR_TYPE_SOA), 
					false);
			if ((fmt->flags & LDNS_COMMENT_LAYOUT))
				fprintf(out, ";\n");
		}

		if (zone->names) {
			ldns_dnssec_zone_names_print_fmt(out, fmt, 
					zone->names, false);
		}
	}
}

void
ldns_dnssec_zone_print(FILE *out, ldns_dnssec_zone *zone)
{
	ldns_dnssec_zone_print_fmt(out, ldns_output_format_default, zone);
}

ldns_status
ldns_dnssec_zone_add_empty_nonterminals(ldns_dnssec_zone *zone)
{
	ldns_dnssec_name *new_name;
	ldns_rdf *cur_name;
	ldns_rdf *next_name;
	ldns_rbnode_t *cur_node, *next_node, *new_node;

	/* for the detection */
	uint16_t i, cur_label_count, next_label_count;
	uint16_t soa_label_count = 0;
	ldns_rdf *l1, *l2;
	int lpos;

	if (!zone) {
		return LDNS_STATUS_ERR;
	}
	if (zone->soa && zone->soa->name) {
		soa_label_count = ldns_dname_label_count(zone->soa->name);
	}
	
	cur_node = ldns_rbtree_first(zone->names);
	while (cur_node != LDNS_RBTREE_NULL) {
		next_node = ldns_rbtree_next(cur_node);
		
		/* skip glue */
		while (next_node != LDNS_RBTREE_NULL && 
		       next_node->data &&
		       ((ldns_dnssec_name *)next_node->data)->is_glue
		) {
			next_node = ldns_rbtree_next(next_node);
		}

		if (next_node == LDNS_RBTREE_NULL) {
			next_node = ldns_rbtree_first(zone->names);
		}

		cur_name = ((ldns_dnssec_name *)cur_node->data)->name;
		next_name = ((ldns_dnssec_name *)next_node->data)->name;
		cur_label_count = ldns_dname_label_count(cur_name);
		next_label_count = ldns_dname_label_count(next_name);

		/* Since the names are in canonical order, we can
		 * recognize empty non-terminals by their labels;
		 * every label after the first one on the next owner
		 * name is a non-terminal if it either does not exist
		 * in the current name or is different from the same
		 * label in the current name (counting from the end)
		 */
		for (i = 1; i < next_label_count - soa_label_count; i++) {
			lpos = (int)cur_label_count - (int)next_label_count + (int)i;
			if (lpos >= 0) {
				l1 = ldns_dname_clone_from(cur_name, (uint8_t)lpos);
			} else {
				l1 = NULL;
			}
			l2 = ldns_dname_clone_from(next_name, i);

			if (!l1 || ldns_dname_compare(l1, l2) != 0) {
				/* We have an empty nonterminal, add it to the
				 * tree
				 */
				new_name = ldns_dnssec_name_new();
				if (!new_name) {
					return LDNS_STATUS_MEM_ERR;
				}
				new_name->name = ldns_dname_clone_from(next_name,
				                                       i);
				if (!new_name->name) {
					ldns_dnssec_name_free(new_name);
					return LDNS_STATUS_MEM_ERR;
				}
				new_name->name_alloced = true;
				new_node = LDNS_MALLOC(ldns_rbnode_t);
				if (!new_node) {
					ldns_dnssec_name_free(new_name);
					return LDNS_STATUS_MEM_ERR;
				}
				new_node->key = new_name->name;
				new_node->data = new_name;
				(void)ldns_rbtree_insert(zone->names, new_node);
			}
			ldns_rdf_deep_free(l1);
			ldns_rdf_deep_free(l2);
		}
		
		/* we might have inserted a new node after
		 * the current one so we can't just use next()
		 */
		if (next_node != ldns_rbtree_first(zone->names)) {
			cur_node = next_node;
		} else {
			cur_node = LDNS_RBTREE_NULL;
		}
	}
	return LDNS_STATUS_OK;
}

bool
ldns_dnssec_zone_is_nsec3_optout(ldns_dnssec_zone* zone)
{
	ldns_rr* nsec3;
	ldns_rbnode_t* node;

	if (ldns_dnssec_name_find_rrset(zone->soa, LDNS_RR_TYPE_NSEC3PARAM)) {
		node = ldns_rbtree_first(zone->names);
		while (node != LDNS_RBTREE_NULL) {
			nsec3 = ((ldns_dnssec_name*)node->data)->nsec;
			if (nsec3 &&ldns_rr_get_type(nsec3) 
					== LDNS_RR_TYPE_NSEC3 &&
					ldns_nsec3_optout(nsec3)) {
				return true;
			}
			node = ldns_rbtree_next(node);
		}
	}
	return false;
}
