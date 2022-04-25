#include <iostream>

// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "tree-pass.h"
#include "context.h"

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cgraph.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "alloc-pool.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"
#include "fibonacci_heap.h"
#include <limits>

int plugin_is_GPL_compatible;

static struct plugin_info C3_plugin_info = {"1.0", "C3 gcc plugin"};

namespace {

    #define C3_CLUSTER_THRESHOLD 1024

    struct cluster_edge;

    struct cluster
    {
        cluster (cgraph_node *node, int size, sreal time):
            m_functions (), m_callers (), m_size (size), m_time (time)
        {
            m_functions.safe_push (node);
        }

        vec<cgraph_node *> m_functions;
        hash_map <cluster *, cluster_edge *> m_callers;
        int m_size;
        sreal m_time;
    };

    struct cluster_edge
    {
        cluster_edge (cluster *caller, cluster *callee, uint32_t count):
            m_caller (caller), m_callee (callee), m_count (count), 
            m_heap_node (NULL) {}

        uint32_t inverted_count ()
        {
            return std::numeric_limits<uint32_t>::max () - m_count;
        }

        cluster *m_caller;
        cluster *m_callee;
        uint32_t m_count;
        fibonacci_node<uint32_t, cluster_edge> *m_heap_node;
    };

    static int cluster_cmp (const void *a_p, const void *b_p)
    {
        const cluster *a = *(cluster * const *)a_p;
        const cluster *b = *(cluster * const *)b_p;

        unsigned fncounta = a->m_functions.length ();
        unsigned fncountb = b->m_functions.length ();
        if (fncounta <= 1 || fncountb <= 1)
            return fncountb - fncounta;

        sreal r = b->m_time * a->m_size - a->m_time * b->m_size;
        return (r < 0) ? -1 : ((r > 0) ? 1 : 0);
    }

    static unsigned int c3_reorder ()
    {
        cgraph_node *node;
        auto_vec<cluster *> clusters;

        /* Create a cluster for each function.  */
        FOR_EACH_DEFINED_FUNCTION (node)
        if (!node->alias && !node->global.inlined_to)
        {
            ipa_fn_summary *summary = ipa_fn_summaries->get (node);
            cluster *c = new cluster (node, summary->size, summary->time);
            node->aux = c;
            clusters.safe_push (c);
        }

        auto_vec<cluster_edge *> edges;

        /* Insert edges between clusters that have a profile.  */
        for (unsigned i = 0; i < clusters.length (); i++)
        {
            cgraph_node *node = clusters[i]->m_functions[0];
            for (cgraph_edge *cs = node->callers; cs; cs = cs->next_caller)
            {
                if (cs->count.reliable_p ()
                    && cs->count.to_gcov_type () > 0)
                {
                    cluster *caller = (cluster *)cs->caller->aux;
                    cluster *callee = (cluster *)cs->callee->aux;
                    gcov_type count = cs->count.to_gcov_type ();

                    cluster_edge **cedge = callee->m_callers.get (caller);
                    if (cedge != NULL)
                        (*cedge)->m_count += count;
                    else
                    {
                        cluster_edge *cedge = new cluster_edge (caller, callee, 
                                                                count);
                        edges.safe_push (cedge);
                        callee->m_callers.put (caller, cedge);
                    }
                }
            }
        }

        /* Now insert all created edges into a heap.  */
        fibonacci_heap <uint32_t, cluster_edge> heap (0);

        for (unsigned i = 0; i < clusters.length (); i++)
        {
            cluster *c = clusters[i];
            for (hash_map<cluster *, cluster_edge *>::iterator it = 
                 c->m_callers.begin (); it != c->m_callers.end (); ++it)
            {
                cluster_edge *cedge = (*it).second;
                cedge->m_heap_node = heap.insert (cedge->inverted_count (), 
                                                  cedge);
            }
        }

        while (!heap.empty ())
        {
            cluster_edge *cedge = heap.extract_min ();
            cluster *caller = cedge->m_caller;
            cluster *callee = cedge->m_callee;
            cedge->m_heap_node = NULL;

            if (caller == callee)
    	        continue;
            if (caller->m_size + callee->m_size <= C3_CLUSTER_THRESHOLD)
    	    {
                caller->m_size += callee->m_size;
                caller->m_time += callee->m_time;

                /* Append all cgraph_nodes from callee to caller.  */
                for (unsigned i = 0; i < callee->m_functions.length (); i++)
                    caller->m_functions.safe_push (callee->m_functions[i]);

                callee->m_functions.truncate (0);

                /* Iterate all cluster_edges of callee and add them to 
                the caller. */
                for (hash_map<cluster *, cluster_edge *>::iterator it = 
                    callee->m_callers.begin (); it != callee->m_callers.end ();
                    ++it)
                {
                    (*it).second->m_callee = caller;
                    cluster_edge **ce = caller->m_callers.get ((*it).first);

                    if (ce != NULL)
                    {
                        (*ce)->m_count += (*it).second->m_count;
                        if ((*ce)->m_heap_node != NULL)
                            heap.decrease_key  ((*ce)->m_heap_node, 
                                                (*ce)->inverted_count ());
                    }
                    else
                        caller->m_callers.put ((*it).first, (*it).second);
                }
            }
        }

        /* Sort the candidate clusters.  */
        clusters.qsort (cluster_cmp);

        /* Dump clusters.  */
        if (dump_file)
        {
            for (unsigned i = 0; i < clusters.length (); i++)
    	    {
    	    cluster *c = clusters[i];
    	    if (c->m_functions.length () <= 1)
    	        continue;

    	    fprintf (dump_file, "Cluster %d with functions: %d, size: %d,"
    		        " density: %f\n", i, c->m_functions.length (), c->m_size,
    		        (c->m_time / c->m_size).to_double ());
    	    fprintf (dump_file, "  functions: ");
    	    for (unsigned j = 0; j < c->m_functions.length (); j++)
    	        fprintf (dump_file, "%s ", c->m_functions[j]->dump_name ());
    	        fprintf (dump_file, "\n");
    	    }
            fprintf (dump_file, "\n");
        }

        /* Assign .text.sorted.* section names.  */
        int counter = 1;
        for (unsigned i = 0; i < clusters.length (); i++)
        {
            cluster *c = clusters[i];
            if (c->m_functions.length () <= 1)
    	        continue;

            for (unsigned j = 0; j < c->m_functions.length (); j++)
    	    {
    	        cgraph_node *node = c->m_functions[j];

    	        if (dump_file)
    	            fprintf (dump_file, "setting: %d for %s with size:%d\n",
    		            counter, node->dump_asm_name (),
    		            ipa_fn_summaries->get (node)->size);
    	        // node->text_sorted_order = counter++;
    	    }
        }

        /* Release memory.  */
        FOR_EACH_DEFINED_FUNCTION (node)
        if (!node->alias)
            node->aux = NULL;

        for (unsigned i = 0; i < clusters.length (); i++)
            delete clusters[i];

        for (unsigned i = 0; i < edges.length (); i++)
            delete edges[i];

        return 0;
    }

    const pass_data pass_data_ipa_reorder =
    {
        IPA_PASS, /* type */
        "c3_reorder", /* name */
        OPTGROUP_NONE, /* optinfo_flags */
        TV_NONE, /* tv_id */
        0, /* properties_required */
        0, /* properties_provided */
        0, /* properties_destroyed */
        0, /* todo_flags_start */
        0, /* todo_flags_finish */
    };

    struct c3_pass: public ipa_opt_pass_d
    {
        c3_pass (gcc::context *ctxt):
            ipa_opt_pass_d (pass_data_ipa_reorder, ctxt,
            		        NULL, /* generate_summary */
 		                    NULL, /* write_summary */
                            NULL, /* read_summary */
                            NULL, /* write_optimization_summary */
                            NULL, /* read_optimization_summary */
                            NULL, /* stmt_fixup */
                            0, /* function_transform_todo_flags_start */
                            NULL, /* function_transform */
                            NULL  /* variable_transform */)
        {}

        virtual unsigned int execute (function *) { return c3_reorder (); }
        virtual bool gate (function *);

    };

    bool c3_pass::gate (function *)
    {
        return flag_profile_reorder_functions && flag_profile_use && flag_wpa;
    }

}

int plugin_init (struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{

    register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, 
                      &C3_plugin_info);

    struct register_pass_info pass_info;

    pass_info.pass = new c3_pass (g); // "g" is a global gcc::context pointer
    pass_info.reference_pass_name       = "pure-const";
    pass_info.ref_pass_instance_number  = 1;
    pass_info.pos_op                    = PASS_POS_INSERT_AFTER;

    register_callback (plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
                       NULL, &pass_info);

    return 0;
}
