#include "i.h"
#include <stdlib.h>

int
main(int argc, char*argv[]) {
	verbosity = 0;
	assert(param_get_integer("parameter_test", "forty_two") == 42);
	assert(param_get_double("parameter_test", "nought_point_five") == 0.5);
	param_set_double("parameter_test", "nought_point_five", 0.49);
	assert(param_get_double("parameter_test", "nought_point_five") == 0.49);
	param_assignment("parameter_test:nought_point_five=0.51");
	assert(param_get_double("parameter_test", "nought_point_five") == 0.51);
//	param_set_double("parameter_test", "not_existing", 12.34);
//	assert(param_get_double("parameter_test", "not_existing") == 12.34);
	return 0;
}
