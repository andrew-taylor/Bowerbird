/*=========================================================================================
 * these are the routines for selecting or dropping channels/tdoaresults/locationestimates
 * according to the various heuristics we compute in each stage of the algorithm.
 *
 * currently there are three criteria which you can use
 * 	1. select or drop based on the value of a heuristic
 * 		(VALUE_ABOVE and VALUE_BELOW)
 * 	2. select or drop based on the absolute position relative to a heuristic
 * 		(POSITION_FROM_TOP, POSITION_FROM_MIDDLE, POSITION_FROM_BOTTOM)
 * 	3. select or drop based on the relative position relative to a heuristic
 * 		(PERCENT_FROM_TOP, PERCENT_FROM_MIDDLE, PERCENT_FROM_BOTTOM)
 * What I mean is this:
 *
 * We have a bunch of data, each of which has a value for a certain heuristic.
 * With VALUE_ABOVE we can select or drop all of the datapoints for which the value of
 * the heuristic is above a certain threshold (and discard/keep the rest).
 *
 * Or. we could sort the data points according to the value of the heuristic and then
 * say take the best 5 data points (e.g. POSITION_FROM_TOP)
 * Or we could take the median point (using e.g. POSITION_FROM_MIDDLE)
 *
 * Finally we could specify a range of points to keep according to a percentile range.
 * e.g. we could take the upper quartile, or middle half, etc.  This is what for example,
 * PERCENT_FROM_TOP is used for.  Note the similarity and difference with the POSITION_*
 * ones.
 *
 *-----------------------------------------------------------------------------------------
 *
 * The basic flow of the algorithm (as seen in analyze_region) is that
 *    1. we start of with the waveforms of some region.  That is we have 12 channels
 *       for a certain region of sound.
 *    2. we filter all of this data.  We thus have 4 channel_t's for each station
 *    
 *    3. we have the option to analyze these filtered waveforms directly, e.g. to analyze
 *       their shape in some way, or to analyze their signal-to-noise-ratio. (for example).
 *       Through this analyze we can obtain some heuristics (i.e. double values) attached
 *       to each channel.
 *
 *    4. we can then drop or select channels for each station according to this information
 *       using the functions defined in this file.
 *
 *    5. we then take the remaining channels and compute all possible TDOAs between them.
 *       this gives us a bunch of tdoa_result_t's for each pair of stations.
 *
 *    6. in the process of computing each tdoa_result_t we can compute some heuristics,
 *       for example, by analyzing the noise or shape of the cross-correlation graph.
 *       
 *    7. then we can drop or select tdoa_result_t's according to all the information we have
 *       so far
 *
 *    8. we then take all remaining tdoa_results and compute location estimates
 *
 *    9. again, we can obtain heuristics about this. 
 *
 * The point is that we have heuristics for each channel_t, tdoaresult_t, and estimate_t.
 * We use the functions of this file to analyze these heuristics for all of these three
 * structures.
 *=========================================================================================*/
#define VALUE_ABOVE		0
#define VALUE_BELOW		1
#define POSITION_FROM_TOP	2
#define POSITION_FROM_MIDDLE	3
#define POSITION_FROM_BOTTOM	4
#define PERCENT_FROM_TOP	5
#define PERCENT_FROM_MIDDLE	6
#define PERCENT_FROM_BOTTOM	7

#include "i.h"

void g_ptr_array_remove_range_and_free(GPtrArray *array,int start, int length) {
	for (int i=start; i<start+length; i++)
	{
		free(g_ptr_array_index(array,i));
	}
	g_ptr_array_remove_range(array,start,length);
}

int select_percentage(GPtrArray *array, int heur_id, int query_type, double percent)
{
	double top_percent, bottom_percent;
	switch (query_type)
	{
	case PERCENT_FROM_TOP:
		top_percent = 1.0;
		bottom_percent = percent;
		break;
	case PERCENT_FROM_MIDDLE:
		top_percent = 0.5+percent/2.0;
		bottom_percent = 0.5-percent/2.0;
		break;
	case PERCENT_FROM_BOTTOM:
		top_percent = percent;
		bottom_percent = 0;
		break;
	default: die("Unsupported query_type for select_percentage");
	}
	int top_idx = array->len*top_percent;
	int bottom_idx = array->len*bottom_percent;
	top_idx = MIN(array->len-1,top_idx);
	bottom_idx = MAX(0,bottom_idx);

	if (top_idx+1 < array->len)
		g_ptr_array_remove_range_and_free(array,top_idx+1,array->len-1-top_idx);
	g_ptr_array_remove_range_and_free(array,0,bottom_idx);

	return (bottom_idx + array->len-1-top_idx);
}
int select_position(GPtrArray *array, int heur_id, int query_type, double threshold)
{
	int top_idx, bottom_idx;
	switch (query_type)
	{
	case POSITION_FROM_TOP:
		top_idx = array->len-1;
		bottom_idx = array->len-1-threshold;
		break;
	case POSITION_FROM_MIDDLE:
		top_idx = array->len/2 + threshold;
		bottom_idx = array->len/2 - threshold;
		break;
	case POSITION_FROM_BOTTOM:
		top_idx = threshold;
		bottom_idx = 0;
		break;
	default: die("Unsupported query_type for select_position");
	}

	top_idx = MIN(top_idx,array->len-1);
	bottom_idx = MAX(bottom_idx,0);

	if (top_idx+1<array->len)
		g_ptr_array_remove_range_and_free(array,top_idx+1,array->len-1-top_idx);
	g_ptr_array_remove_range_and_free(array,0,bottom_idx);

	return (bottom_idx + array->len-1-top_idx);
}


int select_value(GPtrArray *array, int heur_id, int query_type, double threshold)
{
	int pos;
	switch (query_type)
	{
	case VALUE_ABOVE:
		pos=array->len;
		for (int i=array->len-1; i>=0; i--)
		{
//			double value = ((channel_t*)g_ptr_array_index(array,i))->heuristics[heur_id];
			double value = ((double *)g_ptr_array_index(array,i))[heur_id];			
			if (value <= threshold)
				break;
			pos--;
		}
		g_ptr_array_remove_range_and_free(array,0,pos);
		return pos;
	case VALUE_BELOW:
		pos=-1;
		for (int i=0; i<array->len; i++)
		{
			double value = ((channel_t*)g_ptr_array_index(array,i))->heuristics[heur_id];
			if (value >= threshold)
				break;
			pos++;
		}
		if (pos+1 < array->len)
			g_ptr_array_remove_range_and_free(array,pos+1,array->len-pos-1);
		return array->len-pos-1;
	default: die("Unsupported query_type for select_value");
	}
}
double value_percentage(GPtrArray *array, int heur_id, int query_type, double threshold)
{
	double percent;
	switch (query_type)
	{
	case PERCENT_FROM_TOP:
	case PERCENT_FROM_MIDDLE:
	case PERCENT_FROM_BOTTOM:
		percent = threshold;
		break;
	default: die("Unsupported query_type for value_percentage");
	}
	int idx = array->len*percent;
	idx = MIN(array->len-1,idx);
	idx = MAX(0,idx);

	double *elem = (double *)g_ptr_array_index(array,idx);
	return elem[heur_id];
}
double value_position(GPtrArray *array, int heur_id, int query_type, double threshold)
{
	int idx;
	switch (query_type)
	{
	case POSITION_FROM_TOP:
		idx = array->len-1-threshold;
		break;
	case POSITION_FROM_MIDDLE:
		idx = array->len/2;
		break;
	case POSITION_FROM_BOTTOM:
		idx = threshold;
		break;
	default: die("Unsupported query_type for value_position");
	}

	idx = MIN(idx,array->len-1);
	idx = MAX(idx,0);

	double *elem = (double *)g_ptr_array_index(array,idx);
	return elem[heur_id];
}

/* the sort function needs to know which heuristic we are sorting over */
int static_sort_by_heuristic_heur_id;
int cmp_heuristic(const void *p1, const void *p2)
{
/* without the hack, we'd have one of these for each structure, and do
   something like
  
  	channel_t *chan1 = *((channel_t **)p1);
	channel_t *chan2 = *((channel_t **)p2);
	double heur1 = chan1->heuristics[static_sort_channels_heur_id];
	double heur2 = chan2->heuristics[static_sort_channels_heur_id];
   
  instead we just go:
*/
	double heur1 = (*((double **)p1))[static_sort_by_heuristic_heur_id];
	double heur2 = (*((double **)p2))[static_sort_by_heuristic_heur_id];
	if (heur1 == heur2) return 0;
	return (heur1 > heur2) ? 1:-1;
}
/* used to be called sort_channels, since had to have one for each type */
void sort_by_heuristic(GPtrArray *array, int heur_id)
{
	static_sort_by_heuristic_heur_id = heur_id;
	g_ptr_array_sort(array,cmp_heuristic);
}
double value(GPtrArray *array, int heur_id, int query_type, double argument)
{
	sort_by_heuristic(array,heur_id);
	switch (query_type)
	{
	case POSITION_FROM_TOP:
	case POSITION_FROM_MIDDLE:
	case POSITION_FROM_BOTTOM:
		return value_position(array,heur_id,query_type,argument);
	case PERCENT_FROM_TOP:
	case PERCENT_FROM_MIDDLE:
	case PERCENT_FROM_BOTTOM:
		return value_percentage(array,heur_id,query_type,argument);
	default: die("Unsupported query_type for value");
	}
}

void
select_single(GPtrArray *array, int  heur_id, int query_type, double argument)
{
	sort_by_heuristic(array,heur_id);
	switch (query_type)
	{
	case VALUE_ABOVE:
	case VALUE_BELOW:
		select_value(array,heur_id,query_type,argument);
		break;
	case POSITION_FROM_TOP:
	case POSITION_FROM_MIDDLE:
	case POSITION_FROM_BOTTOM:
		select_position(array,heur_id,query_type,argument);
		break;
	case PERCENT_FROM_TOP:
	case PERCENT_FROM_MIDDLE:
	case PERCENT_FROM_BOTTOM:
		select_percentage(array,heur_id,query_type,argument);
		break;
	default: die("Unsupported query_type for select_single");
	}
}

void select_main(GPtrArray *array[NUM_STATIONS], int  heur_id, int query_type, double argument)
{
	for (int i=0; i<NUM_STATIONS; i++)
	{
		select_single(array[i],heur_id,query_type,argument);
	}
}
int drop_single(GPtrArray *array, int  heur_id, int query_type, double argument)
{
	sort_by_heuristic(array,heur_id);
	switch (query_type)
	{
	case VALUE_ABOVE:
		return select_value(array,heur_id,VALUE_BELOW,argument+DBL_EPSILON);
	case VALUE_BELOW:
		return select_value(array,heur_id,VALUE_ABOVE,argument-DBL_EPSILON);
	case POSITION_FROM_TOP:
		return select_position(array,heur_id,POSITION_FROM_BOTTOM,array->len - argument);
	case POSITION_FROM_BOTTOM:
		return select_position(array,heur_id,POSITION_FROM_TOP,array->len - argument);
	case PERCENT_FROM_TOP:
		return select_percentage(array,heur_id,PERCENT_FROM_BOTTOM,1-argument+1.0/array->len);
	case PERCENT_FROM_BOTTOM:
		return select_percentage(array,heur_id,PERCENT_FROM_TOP,1-argument+1.0/array->len);
	default: die("Unsupported query_type for drop");
	}
}
void drop(GPtrArray *array[NUM_STATIONS], int heur_id, int query_type, double argument)
{
	for (int i=0; i<NUM_STATIONS; i++)
	{
		drop_single(array[i],heur_id,query_type,argument);
	}
}
