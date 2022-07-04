/**
 * @file c3-ipa.cpp
 */

/*
 * This plugin is a truncated version of Martin Liska's c3-pass
 * The original sources you may see on
 * https://gcc.gnu.org/legacy-ml/gcc-patches/2019-09/msg01142.html
 * */

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

#include <fstream>
#include "include/perf_parser.hpp"
#include "include/funcData.hpp"

int plugin_is_GPL_compatible; ///<To prove our dedication to free software

static struct plugin_info C3_plugin_info = {"1.0", "c3_reorder"};

namespace {


    static int func_cmp (const void *a_p, const void *b_p)
    {
        cgraph_node *a = *(cgraph_node **)a_p;
        cgraph_node *b = *(cgraph_node **)b_p;
        return strcmp(a->name(), b->name());
    }

    void parse_lbr_perf_data (std::vector<perfParser::LbrSample> lbrParse, const char* perf_script_path) {

        perfParser::TraceStream traceReader (perf_script_path);
        std::vector<std::pair<std::string, std::string>> lbrSamplesPreRecord;

        while (!traceReader.isAtEOF ()) {

            while (!traceReader.getCurrentLine ().size ())
                traceReader.advance ();

            std::string curStr = boost::trim_copy (traceReader.getCurrentLine ());
            perfParser::lbrPreParse (lbrSamplesPreRecord, curStr);

            traceReader.advance ();

        }

        perfParser::lbrSampleReParse (lbrParse, lbrSamplesPreRecord);

        // for (auto el: lbrParse)
            // std::cerr << "\"" << el.callerName_ << "\" (0x" << std::hex << el.callerOffset_ << ") -> \"" << el.calleeName_ << "\"" << std::endl; 

    }

    void parse_hybrid_perf_data (const char* perf_script_path) {

        //!TODO

    }

    /**
     * @brief c3 reorder itself
     * @return error code
     */
    static unsigned int c3_reorder (const char* perf_script_path) {

        perfParser::PerfContent type =  perfParser::checkPerfScriptType (perf_script_path);
        switch (type) {
            case perfParser::PerfContent::Unknown:
                throw std::runtime_error ("Unknown perf script file format");
            case perfParser::PerfContent::LBR: {
                std::vector<perfParser::LbrSample> lbrParse;
                parse_lbr_perf_data (lbrParse, perf_script_path);
                break;
            }
            case perfParser::PerfContent::LBRStack:
                parse_hybrid_perf_data (perf_script_path);
                break;
            default: break;
            
        }

        return 0;

    }
    // static unsigned int c3_reorder (const char* perf_script_path)
    // {
    //     FILE * fp;
    //     fp = fopen ("/home/exactlywb/Desktop/ISP_RAS/c3_reorder_plugin/PASS_WORK.txt", "a+"); //PLEASE, CHANGE IT
    //     cgraph_node *node;
    //     auto_vec<cgraph_node *> functions;

    //     fprintf(fp, "Start c3_ipa plugin\n");
    //     /* Create a cluster for each function.  */
    //     FOR_EACH_DEFINED_FUNCTION (node) {
    //       if(node == nullptr) continue;
    //       if (!node->alias && !node->global.inlined_to)
    //       {
    //           //fprintf(fp, "func: %s\n", node->name());
    //           functions.safe_push (node);
    //       }
    //     }

    //     /* Sort the candidate clusters.  */
    //     functions.qsort (func_cmp);
    //     for (int i = 0; i < functions.length(); i++) {
    //       functions[i]->text_sorted_order = i;
    //       fprintf(fp, "[%d] %s\n", i, functions[i]->asm_name  ());
    //     }

    //     fclose(fp);

    //     return 0;
    // }

    ///passage info
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

    /**
     * @brief the executable passage
     */
    struct c3_pass: public ipa_opt_pass_d
    {
        c3_pass (gcc::context *ctxt, const char* perf_path):
            ipa_opt_pass_d (pass_data_ipa_reorder, ctxt,
            		        NULL, /* generate_summary */
 		                    NULL, /* write_summary */
                            NULL, /* read_summary */
                            NULL, /* write_optimization_summary */
                            NULL, /* read_optimization_summary */
                            NULL, /* stmt_fixup */
                            0, /* function_transform_todo_flags_start */
                            NULL, /* function_transform */
                            NULL  /* variable_transform */),
            perf_script_path (perf_path)
        {}

        /**
         * @brief The execute method
         * @warning do not call it by yourself
         */
        virtual unsigned int execute (function *) { return c3_reorder (perf_script_path); }

        /**
         * @brief This function is responsible for when the passage
         * should work and when not
         */
        virtual bool gate (function *);

    private:
        const char* perf_script_path = NULL;

    };

    bool c3_pass::gate (function *)
    {
        //return flag_profile_reorder_functions && flag_profile_use && flag_wpa;
        return flag_reorder_functions && flag_wpa;
    }

    void check_arguments (struct plugin_name_args *plugin_info) {

        const int argc = plugin_info->argc;
        const struct plugin_argument *argv = plugin_info->argv;

        if (argc != 1)
            throw std::runtime_error ("wrong number of arguments in c3_reorder plugin");

        #define perf_data_length 11
        if (strncmp (argv [0].key, "perf_script", perf_data_length))
            throw std::runtime_error ("perf_script argument expected");

        const char* perf_script_path = argv [0].value;

        std::ifstream test_exist (perf_script_path);
        if (!test_exist.is_open ())
            throw std::runtime_error ("bad path for perf_script file");

        test_exist.close ();

    }

}

/**
 * @brief The main plugin function hooking the desired passage
 * @param[in] plugin_info Plugin invocation information
 * @param[in] version GCC version
 * @return error code
 */
int plugin_init (struct plugin_name_args *plugin_info,
	        	 struct plugin_gcc_version *version)
{

    if (!plugin_default_version_check (version, &gcc_version))
    {
        std::cerr << "Bad gcc version to compile the plugin" << std::endl;
        return 1;
    }

    register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, 
                      &C3_plugin_info);

    try { 
        check_arguments (plugin_info); 
    } catch (const std::runtime_error& err) {
        std::cerr << "[PLUGIN ERROR]: " << err.what () << std::endl;
        return -1;
    }


    struct register_pass_info pass_info;

    pass_info.pass = new c3_pass (g, plugin_info->argv [0].value); // "g" is a global gcc::context pointer
    pass_info.reference_pass_name       = "pure-const";
    pass_info.ref_pass_instance_number  = 1;
    pass_info.pos_op                    = PASS_POS_INSERT_AFTER;

    register_callback (plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
                       NULL, &pass_info);

    return 0;
}
