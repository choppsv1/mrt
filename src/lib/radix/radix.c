/*
 * $Id: radix.c,v 1.1.1.1 2000/08/14 18:46:13 labovit Exp $
 */

#include <mrt.h>
#include <radix.h>

/* #define RADIX_DEBUG 1 */

static int num_active_radix = 0;

/* these routines support continuous mask only */

radix_tree_t *
New_Radix (int maxbits)
{
    radix_tree_t *radix = New (radix_tree_t);

    radix->maxbits = maxbits;
    radix->head = NULL;
    radix->num_active_node = 0;
    assert (maxbits <= RADIX_MAXBITS); /* XXX */
    num_active_radix++;
    return (radix);
}


/*
 * if func is supplied, it will be called as func(node->data)
 * before deleting the node
 */

void
Clear_Radix (radix_tree_t *radix, void_fn_t func)
{
    assert (radix);
    if (radix->head) {

        radix_node_t *Xstack[RADIX_MAXBITS+1];
        radix_node_t **Xsp = Xstack;
        radix_node_t *Xrn = radix->head;

        while (Xrn) {
            radix_node_t *l = Xrn->l;
            radix_node_t *r = Xrn->r;

    	    if (Xrn->prefix) {
		Deref_Prefix (Xrn->prefix);
		if (Xrn->data && func)
	    	    func (Xrn->data);
    	    }
    	    else {
		assert (Xrn->data == NULL);
    	    }
    	    Delete (Xrn);
	    radix->num_active_node--;

            if (l) {
                if (r) {
                    *Xsp++ = r;
                }
                Xrn = l;
            } else if (r) {
                Xrn = r;
            } else if (Xsp != Xstack) {
                Xrn = *(--Xsp);
            } else {
                Xrn = (radix_node_t *) 0;
            }
        }
    }
    assert (radix->num_active_node == 0);
    /* Delete (radix); */
}


void
Destroy_Radix (radix_tree_t *radix, void_fn_t func)
{
    Clear_Radix (radix, func);
    Delete (radix);
    num_active_radix--;
}


/*
 * if func is supplied, it will be called as func(node->prefix, node->data)
 */

void
radix_process (radix_tree_t *radix, void_fn_t func)
{
    radix_node_t *node;
    assert (func);

    RADIX_WALK (radix->head, node) {
	func (node->prefix, node->data);
    } RADIX_WALK_END;
}


radix_node_t *
radix_search_exact (radix_tree_t *radix, prefix_t *prefix)
{
    radix_node_t *node;
    u_char *addr;
    u_int bitlen;

    assert (radix);
    assert (prefix);
    assert (prefix->bitlen <= radix->maxbits);

    if (radix->head == NULL)
	return (NULL);

    node = radix->head;
    addr = prefix_touchar (prefix);
    bitlen = prefix->bitlen;

    while (node->bit < bitlen) {

	if (BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
#ifdef RADIX_DEBUG
	    if (node->prefix)
    	        fprintf (stderr, "radix_search_exact: take right %s/%d\n", 
	                 prefix_toa (node->prefix), node->prefix->bitlen);
	    else
    	        fprintf (stderr, "radix_search_exact: take right at %d\n", 
			 node->bit);
#endif /* RADIX_DEBUG */
	    node = node->r;
	}
	else {
#ifdef RADIX_DEBUG
	    if (node->prefix)
    	        fprintf (stderr, "radix_search_exact: take left %s/%d\n", 
	                 prefix_toa (node->prefix), node->prefix->bitlen);
	    else
    	        fprintf (stderr, "radix_search_exact: take left at %d\n", 
			 node->bit);
#endif /* RADIX_DEBUG */
	    node = node->l;
	}

	if (node == NULL)
	    return (NULL);
    }

#ifdef RADIX_DEBUG
    if (node->prefix)
        fprintf (stderr, "radix_search_exact: stop at %s/%d\n", 
	         prefix_toa (node->prefix), node->prefix->bitlen);
    else
        fprintf (stderr, "radix_search_exact: stop at %d\n", node->bit);
#endif /* RADIX_DEBUG */
    if (node->bit > bitlen || node->prefix == NULL)
	return (NULL);
    assert (node->bit == bitlen);
    assert (node->bit == node->prefix->bitlen);
    if (comp_with_mask (prefix_tochar (node->prefix), prefix_tochar (prefix),
			bitlen)) {
#ifdef RADIX_DEBUG
        fprintf (stderr, "radix_search_exact: found %s/%d\n", 
	         prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	return (node);
    }
    return (NULL);
}


/* if inclusive != 0, "best" may be the given prefix itself */
radix_node_t *
radix_search_best2 (radix_tree_t *radix, prefix_t *prefix, int inclusive)
{
    radix_node_t *node;
    radix_node_t *stack[RADIX_MAXBITS + 1];
    u_char *addr;
    u_int bitlen;
    int cnt = 0;

    assert (radix);
    assert (prefix);
    assert (prefix->bitlen <= radix->maxbits);

    if (radix->head == NULL)
	return (NULL);

    node = radix->head;
    addr = prefix_touchar (prefix);
    bitlen = prefix->bitlen;

    while (node->bit < bitlen) {

	if (node->prefix) {
#ifdef RADIX_DEBUG
            fprintf (stderr, "radix_search_best: push %s/%d\n", 
	             prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	    stack[cnt++] = node;
	}

	if (BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
#ifdef RADIX_DEBUG
	    if (node->prefix)
    	        fprintf (stderr, "radix_search_best: take right %s/%d\n", 
	                 prefix_toa (node->prefix), node->prefix->bitlen);
	    else
    	        fprintf (stderr, "radix_search_best: take right at %d\n", 
			 node->bit);
#endif /* RADIX_DEBUG */
	    node = node->r;
	}
	else {
#ifdef RADIX_DEBUG
	    if (node->prefix)
    	        fprintf (stderr, "radix_search_best: take left %s/%d\n", 
	                 prefix_toa (node->prefix), node->prefix->bitlen);
	    else
    	        fprintf (stderr, "radix_search_best: take left at %d\n", 
			 node->bit);
#endif /* RADIX_DEBUG */
	    node = node->l;
	}

	if (node == NULL)
	    break;
    }

    if (inclusive && node && node->prefix)
	stack[cnt++] = node;

#ifdef RADIX_DEBUG
    if (node == NULL)
        fprintf (stderr, "radix_search_best: stop at null\n");
    else if (node->prefix)
        fprintf (stderr, "radix_search_best: stop at %s/%d\n", 
	         prefix_toa (node->prefix), node->prefix->bitlen);
    else
        fprintf (stderr, "radix_search_best: stop at %d\n", node->bit);
#endif /* RADIX_DEBUG */

    if (cnt <= 0)
	return (NULL);

    while (--cnt >= 0) {
	node = stack[cnt];
#ifdef RADIX_DEBUG
        fprintf (stderr, "radix_search_best: pop %s/%d\n", 
	         prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	if (comp_with_mask (prefix_tochar (node->prefix), 
			    prefix_tochar (prefix),
			    node->prefix->bitlen)) {
#ifdef RADIX_DEBUG
            fprintf (stderr, "radix_search_best: found %s/%d\n", 
	             prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	    return (node);
	}
    }
    return (NULL);
}


radix_node_t *
radix_search_best (radix_tree_t *radix, prefix_t *prefix)
{
    return (radix_search_best2 (radix, prefix, 1));
}


radix_node_t *
radix_lookup (radix_tree_t *radix, prefix_t *prefix)
{
    radix_node_t *node, *new_node, *parent, *glue;
    u_char *addr, *test_addr;
    u_int bitlen, check_bit, differ_bit;
    int i, j, r;

    assert (radix);
    assert (prefix);
    assert (prefix->bitlen <= radix->maxbits);

    if (radix->head == NULL) {
	node = New (radix_node_t);
	node->bit = prefix->bitlen;
	node->prefix = Ref_Prefix (prefix);
	node->parent = NULL;
	node->l = node->r = NULL;
	node->data = NULL;
	radix->head = node;
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_lookup: new_node #0 %s/%d (head)\n", 
		 prefix_toa (prefix), prefix->bitlen);
#endif /* RADIX_DEBUG */
	radix->num_active_node++;
	return (node);
    }

    addr = prefix_touchar (prefix);
    bitlen = prefix->bitlen;
    node = radix->head;

    while (node->bit < bitlen || node->prefix == NULL) {

	if (node->bit < radix->maxbits &&
	    BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
	    if (node->r == NULL)
		break;
#ifdef RADIX_DEBUG
	    if (node->prefix)
    	        fprintf (stderr, "radix_lookup: take right %s/%d\n", 
	                 prefix_toa (node->prefix), node->prefix->bitlen);
	    else
    	        fprintf (stderr, "radix_lookup: take right at %d\n", node->bit);
#endif /* RADIX_DEBUG */
	    node = node->r;
	}
	else {
	    if (node->l == NULL)
		break;
#ifdef RADIX_DEBUG
	    if (node->prefix)
    	        fprintf (stderr, "radix_lookup: take left %s/%d\n", 
	             prefix_toa (node->prefix), node->prefix->bitlen);
	    else
    	        fprintf (stderr, "radix_lookup: take left at %d\n", node->bit);
#endif /* RADIX_DEBUG */
	    node = node->l;
	}

	assert (node);
    }

    assert (node->prefix);
#ifdef RADIX_DEBUG
    fprintf (stderr, "radix_lookup: stop at %s/%d\n", 
	     prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */

    test_addr = prefix_touchar (node->prefix);
    /* find the first bit different */
    check_bit = (node->bit < bitlen)? node->bit: bitlen;
    differ_bit = 0;
    for (i = 0; i*8 < check_bit; i++) {
	if ((r = (addr[i] ^ test_addr[i])) == 0) {
	    differ_bit = (i + 1) * 8;
	    continue;
	}
	/* I know the better way, but for now */
	for (j = 0; j < 8; j++) {
	    if (BIT_TEST (r, (0x80 >> j)))
		break;
	}
	/* must be found */
	assert (j < 8);
	differ_bit = i * 8 + j;
	break;
    }
    if (differ_bit > check_bit)
	differ_bit = check_bit;
#ifdef RADIX_DEBUG
    fprintf (stderr, "radix_lookup: differ_bit %d\n", differ_bit);
#endif /* RADIX_DEBUG */

    parent = node->parent;
    while (parent && parent->bit >= differ_bit) {
	node = parent;
	parent = node->parent;
#ifdef RADIX_DEBUG
	if (node->prefix)
            fprintf (stderr, "radix_lookup: up to %s/%d\n", 
	             prefix_toa (node->prefix), node->prefix->bitlen);
	else
            fprintf (stderr, "radix_lookup: up to %d\n", node->bit);
#endif /* RADIX_DEBUG */
    }

    if (differ_bit == bitlen && node->bit == bitlen) {
	if (node->prefix) {
#ifdef RADIX_DEBUG 
    	    fprintf (stderr, "radix_lookup: found %s/%d\n", 
		     prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	    return (node);
	}
	node->prefix = Ref_Prefix (prefix);
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_lookup: new node #1 %s/%d (glue mod)\n",
		 prefix_toa (prefix), prefix->bitlen);
#endif /* RADIX_DEBUG */
	assert (node->data == NULL);
	return (node);
    }

    new_node = New (radix_node_t);
    new_node->bit = prefix->bitlen;
    new_node->prefix = Ref_Prefix (prefix);
    new_node->parent = NULL;
    new_node->l = new_node->r = NULL;
    new_node->data = NULL;
    radix->num_active_node++;

    if (node->bit == differ_bit) {
	new_node->parent = node;
	if (node->bit < radix->maxbits &&
	    BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
	    assert (node->r == NULL);
	    node->r = new_node;
	}
	else {
	    assert (node->l == NULL);
	    node->l = new_node;
	}
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_lookup: new_node #2 %s/%d (child)\n", 
		 prefix_toa (prefix), prefix->bitlen);
#endif /* RADIX_DEBUG */
	return (new_node);
    }

    if (bitlen == differ_bit) {
	if (bitlen < radix->maxbits &&
	    BIT_TEST (test_addr[bitlen >> 3], 0x80 >> (bitlen & 0x07))) {
	    new_node->r = node;
	}
	else {
	    new_node->l = node;
	}
	new_node->parent = node->parent;
	if (node->parent == NULL) {
	    assert (radix->head == node);
	    radix->head = new_node;
	}
	else if (node->parent->r == node) {
	    node->parent->r = new_node;
	}
	else {
	    node->parent->l = new_node;
	}
	node->parent = new_node;
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_lookup: new_node #3 %s/%d (parent)\n", 
		 prefix_toa (prefix), prefix->bitlen);
#endif /* RADIX_DEBUG */
    }
    else {
        glue = New (radix_node_t);
        glue->bit = differ_bit;
        glue->prefix = NULL;
        glue->parent = node->parent;
        glue->data = NULL;
        radix->num_active_node++;
	if (differ_bit < radix->maxbits &&
	    BIT_TEST (addr[differ_bit >> 3], 0x80 >> (differ_bit & 0x07))) {
	    glue->r = new_node;
	    glue->l = node;
	}
	else {
	    glue->r = node;
	    glue->l = new_node;
	}
	new_node->parent = glue;

	if (node->parent == NULL) {
	    assert (radix->head == node);
	    radix->head = glue;
	}
	else if (node->parent->r == node) {
	    node->parent->r = glue;
	}
	else {
	    node->parent->l = glue;
	}
	node->parent = glue;
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_lookup: new_node #4 %s/%d (glue+node)\n", 
		 prefix_toa (prefix), prefix->bitlen);
#endif /* RADIX_DEBUG */
    }
    return (new_node);
}


void
radix_remove (radix_tree_t *radix, radix_node_t *node)
{
    radix_node_t *parent, *child;

    assert (radix);
    assert (node);

    if (node->r && node->l) {
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_remove: #0 %s/%d (r & l)\n", 
		 prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	
	/* this might be a placeholder node -- have to check and make sure
	 * there is a prefix aossciated with it ! */
	if (node->prefix != NULL) 
	  Deref_Prefix (node->prefix);
	node->prefix = NULL;
	/* Also I needed to clear data pointer -- masaki */
	node->data = NULL;
	return;
    }

    if (node->r == NULL && node->l == NULL) {
#ifdef RADIX_DEBUG
	fprintf (stderr, "radix_remove: #1 %s/%d (!r & !l)\n", 
		 prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
	parent = node->parent;
	Deref_Prefix (node->prefix);
	Delete (node);
        radix->num_active_node--;

	if (parent == NULL) {
	    assert (radix->head == node);
	    radix->head = NULL;
	    return;
	}

	if (parent->r == node) {
	    parent->r = NULL;
	    child = parent->l;
	}
	else {
	    assert (parent->l == node);
	    parent->l = NULL;
	    child = parent->r;
	}

	if (parent->prefix)
	    return;

	/* we need to remove parent too */

	if (parent->parent == NULL) {
	    assert (radix->head == parent);
	    radix->head = child;
	}
	else if (parent->parent->r == parent) {
	    parent->parent->r = child;
	}
	else {
	    assert (parent->parent->l == parent);
	    parent->parent->l = child;
	}
	child->parent = parent->parent;
	Delete (parent);
        radix->num_active_node--;
	return;
    }

#ifdef RADIX_DEBUG
    fprintf (stderr, "radix_remove: #2 %s/%d (r ^ l)\n", 
	     prefix_toa (node->prefix), node->prefix->bitlen);
#endif /* RADIX_DEBUG */
    if (node->r) {
	child = node->r;
    }
    else {
	assert (node->l);
	child = node->l;
    }
    parent = node->parent;
    child->parent = parent;

    Deref_Prefix (node->prefix);
    Delete (node);
    radix->num_active_node--;

    if (parent == NULL) {
	assert (radix->head == node);
	radix->head = child;
	return;
    }

    if (parent->r == node) {
	parent->r = child;
    }
    else {
        assert (parent->l == node);
	parent->l = child;
    }
}
