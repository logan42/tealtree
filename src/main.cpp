#include "cost_function.h"
#include "feature.h"
#include "line_reader.h"
#include "log_trivial.h"
#include "options.h"
#include "raw_feature_histogram.h"
#include "regression_cost_function.h"
#include "trainer.h"
#include "tsv_reader.h"
#include "workflow.h"

#include <iostream>

int main(int argc, const char * argv[]) 
{
    try {
        init_logger();
        parse_options(argc, argv);
        Workflow workflow(options);
        workflow.run();
    }
    catch (const std::exception & ex) {
        std::cerr << "TealTree failed with exception:" << std::endl;
        std::cerr << ex.what() << std::endl;
        std::cerr.flush();
        destroy_logger();
        exit(1);
    }
    destroy_logger();
    return 0;
}
