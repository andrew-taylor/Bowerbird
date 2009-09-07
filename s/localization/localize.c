/*             Ground Parrot Localization Software
 *
 *                  originally written by 
 *                      Beren Sanders
 *                 beren.sanders@gmail.com
 *                   in March/April 2008
 *
 * It uses:
 *   o glib2 for some generic datastructures;
 *   o libsndfile for reading and writing .WAV files;
 *   o libfftw3 for Fourier transforms;
 * And requires:
 *   o sox for some signal processing; 
 *   o gnuplot and python (with pylab) for some plotting functions;
 *   o wavpack for compression (including libwavpack)
 *
 * Things that could be done to improve the software:
 *   o use a library for the signal filtering rather than calling
 *     the external sox executable
 *   o figure out a nicer way to utilize the plotting features of
 *     pylab, or write a more general interface to them (like
 *     the gnuplotf function)
 *   o the gnuplotf function sometimes breaks (it may have something
 *     to do with arrays of floats in being written somewhere in 
 *     exponential notation?) this was one reason why the python
 *     plotting routines were added)
 *   o cleanup the use of radians and degrees in latitude and
 *     longitude coordinates.  the current hodgepodge way of
 *     using both could lead to bugs
 *
 * General comments:
 *
 * The localization system requires a config file which specifies
 * (among other things) the location of three directories, one
 * for each station.  These directories contain numerous one-minute
 * long wave files.  Each wave file contains 4 channels (one for
 * each microphone at the station).  The filenames are of the form
 *                 1205910013.409933@40.wav
 * where 1205910013.409933 is the time at the end of the file (in
 * seconds since epoch) and the @40 signifies the likelihood of this
 * minute of sound data containing ground parrot calls.
 *
 * Each data directory should also contain a subdirectory 
 * `clicktracks'. As the system is used on this data it will extract
 * the PPS signal from the wave files it considers and store these
 * so-called clicktracks in this directory.  Once a given clicktrack
 * has been generated, it will be reused in later runs.
 *
 * The config file specifies the location of the three data
 * directories, the GPS coordinates of the three stations, and a
 * couple of other minor filenames and variables.
 *
 * The input of the localization algorithm is a region of time 
 * covered by the data. Regions of time are specified just by
 * giving a start time and the length of the region (in seconds)
 *
 * The start time of a region can be given in one of two formats:
 *   1) as a real-world time, e.g.  19:30:00.2
 *      [.2 of a second past 7:30 pm] OR
 *   2) as seconds since epoch.  however, you don't need to give
 *      the full number of seconds, but rather can just specify
 *      the least-significant digits of an epoch time
 *      [e.g. if you just give 8201 then it could be expanded
 *       to the full time of 1205918201 seconds since epoch]
 *
 * Within the system all times are given as seconds since epoch.
 * In the user doesn't specify the full time since epoch (or
 * uses real-world time) then to make the conversion we look at
 * the earliest time of a file in the data directory.  This gives
 * us, for instance, the day which the 7:30pm refers to.
 * Thus, there could be some conversion problems if the data dir
 * contains overlapping days of data.  However, there will never
 * be a problem if times are specified in a full epoch string.
 *
 *
 * The program is invoked in one of two ways.  The first way is 
 * just to specify a single region (start time and length) on the
 * command line.  The second way is to specify a file on the command
 * line.  This file will list a whole number of regions which will
 * all be processed.  See the example file for its format.  
 * One advantage of using the batch file is that you can specify
 * the correct, actual locations of test data so that the program
 * can do certain things.
 *
 */


/* Strange things:
 *  o GCC_PLAIN far outperforms GCC_PHAT in computing cross-correlation. Reason?
 *  o Is something inherent in the algorithm causing the poor results for calls
 *    outside of the convex hull of the three stations, or are these errors just
 *    a consequence of poorer signals (since they will be far from one or two stations)?
 */

/* TODO: 
 *
 */

#include "i.h"

/* this is the name of the group for localization parameters in the config file */
#define LOCALIZATION_PARAM_GROUP	"localization"

/* some parameters from the config file */
double BREATHING_SPACE;

/* verbosity for the various graphs that can be displayed */
int graphing;

/* stores the actual known position for the call we are currently
 * processing (if known).  it is very useful for debugging purposes.
 * it should reside in process_file() but it's more convenient out here. */
earthpos_t *current = NULL;


void load_station_positions(earthpos_t stations[])
{
	char *station_position[NUM_STATIONS];
	station_position[0] = param_get_string(LOCALIZATION_PARAM_GROUP,"station0_position");
	station_position[1] = param_get_string(LOCALIZATION_PARAM_GROUP,"station1_position");
	station_position[2] = param_get_string(LOCALIZATION_PARAM_GROUP,"station2_position");
	for (int i=0; i<NUM_STATIONS; i++)
	{
		char lat_char, lng_char;
		double lat_deg, lat_min, lng_deg, lng_min;
		sscanf(station_position[i],"%c%lf %lf %c%lf %lf",&lat_char,&lat_deg,&lat_min,&lng_char,&lng_deg,&lng_min);
		lat_deg = geochar_sgn(lat_char)*lat_deg;
		lng_deg = geochar_sgn(lng_char)*lng_deg;

		set_earthpos(&stations[i],lat_deg,lat_min,lng_deg,lng_min);

		dprintf(10,"Station %d at (Lat %f %f,Lng %f %f)\n",i,stations[i].lat_deg,stations[i].lat_min,stations[i].lng_deg,stations[i].lng_min);
	}

	dprintf(10,"Distance between station 0 and station 1 is %g metres\n",earth_distance(&stations[0],&stations[1]));
	dprintf(10,"Distance between station 1 and station 2 is %g metres\n",earth_distance(&stations[1],&stations[2]));
	dprintf(10,"Distance between station 2 and station 0 is %g metres\n",earth_distance(&stations[2],&stations[0]));
}


/* analyzes the specified region and returns an estimated location */
estimate_t *analyze_region(double start_in_seconds, double length_in_seconds)
{
	dprintf(1,
"=========================================================\n\
 Analyzing region of length %.3lf starting at %.3lf\n\
=========================================================\n",length_in_seconds,start_in_seconds);

	/*===============================================================
	 * first we grab the appropriate waveforms
	 *==============================================================*/

	/* we will actually grab a slightly larger region than the one specified */
	start_in_seconds  = start_in_seconds - BREATHING_SPACE;
	length_in_seconds = length_in_seconds + 2*BREATHING_SPACE;
	double *waveforms[NUM_STATIONS][NUM_CHANNELS];
	for (int i=0; i<NUM_STATIONS; i++)
	{
		grab_station_waveforms(waveforms[i],i,start_in_seconds,length_in_seconds);
	}
	dprintf(5,"Successfully grabbed all waveforms.\n");

	index_t nsamples = length_in_seconds * SAMPLING_RATE;


	GPtrArray *channels[NUM_STATIONS];
	for (int i=0; i<NUM_STATIONS; i++)
	{
		channels[i] = g_ptr_array_new();
		for (int j=0; j<NUM_CHANNELS; j++)
		{
			channel_t *chan = salloc(sizeof(channel_t));
			chan->station = i;
			chan->channel = j;
			chan->waveform = waveforms[i][j];
			chan->nsamples = nsamples;
			g_ptr_array_add(channels[i],chan);
		}
	}

	/*====================================================
	 * then we filter them
	 *====================================================*/
	double *filtered[NUM_STATIONS][NUM_CHANNELS];
	for (int i=0; i<NUM_STATIONS; i++)
	for (int j=0; j<NUM_CHANNELS; j++)
	{
		channel_t *chan = g_ptr_array_index(channels[i],j);
		chan->waveform = filter(chan->waveform,chan->nsamples,3000,5000);
		filtered[i][j] = chan->waveform;
	}
	dprintf(5,"Filtered the waveforms.\n");

	/* we are now done with the original waveforms */
	for (int i=0; i<NUM_STATIONS; i++)
	for (int j=0; j<NUM_CHANNELS; j++)
	{
		free(waveforms[i][j]);
	}

	/*====================================================
	 * then we analyze the filtered waveforms
	 * (for shape, noise, whatever)
	 *====================================================*/

			/* not implemented */

	/* we might do something like this: 
	
	for (int i=0; i<NUM_STATIONS; i++)
	for (int j=0; j<channels[i]->len; j++)
	{
		analyze_channel_noise(g_ptr_array_index(channels[i],j));
	}
	
	*/
	


	/*===============================================================
	 * discard or select channels based just on the filtered waveforms
	 *=============================================================*/

			 /* not implemented */

	/* e.g.
	 	    select_main(channels,CHANNEL_NOISE_HEUR,VALUE_BELOW,1000);
	   or to just select the least noisy channel
		    select_main(channels,CHANNEL_NOISE_HEUR,POSITION_FROM_TOP,0);
	   */

	/*================================================================
	 * then we compute TDOAs for all combinations of remaining channels
	 *===============================================================*/
	int method = GCC_PLAIN;
	GPtrArray *tdoa_results[NUM_STATIONS];
	for (int s1=0; s1<NUM_STATIONS; s1++)
	{
		int s2=(s1+1)%NUM_STATIONS;
		tdoa_results[s1] = g_ptr_array_new();

		for (int c1=0; c1<channels[s1]->len; c1++)
		for (int c2=0; c2<channels[s2]->len; c2++)
		{
			channel_t *chan1 = g_ptr_array_index(channels[s1],c1);
			channel_t *chan2 = g_ptr_array_index(channels[s2],c2);

			tdoa_result_t *result = salloc(sizeof(tdoa_result_t));
			result->channel1 = chan1; 
			result->channel2 = chan2;
			result->tdoa = time_difference_of_arrival(chan1->waveform,chan2->waveform,nsamples,method,result->heuristics);
			g_ptr_array_add(tdoa_results[s1],result);
		}
	}

	/* we're then done with the filtered waveforms  */
	for (int i=0; i<NUM_STATIONS; i++)
	for (int j=0; j<NUM_CHANNELS; j++)
	{
		free(filtered[i][j]);
	}

	/*===========================================================
	 * discard or select TDOA results based on TDOA heuristics
	 *==========================================================*/
	//e.g. drop(tdoa_results,TDOA_BADNESS_HEUR,VALUE_BELOW,1000);
	//the following work:
	select_main(tdoa_results,TDOA_TDOA_HEUR,POSITION_FROM_MIDDLE,0);  /* to grab the medians */
	//select_main(tdoa_results,TDOA_TDOA_HEUR,PERCENT_FROM_MIDDLE,0.5);   /* to drop the outer two quartiles */
	

	/*===========================================================
	 * then we do the geometric algorithm for all combinations of
	 * remaining TDOAs
	 *==========================================================*/
	earthpos_t station_earthpos[NUM_STATIONS];
	load_station_positions(station_earthpos);
	GPtrArray *estimates = g_ptr_array_new();

	for (int i0=0; i0<tdoa_results[0]->len; i0++)
	for (int i1=0; i1<tdoa_results[1]->len; i1++)
	for (int i2=0; i2<tdoa_results[2]->len; i2++)
	{
		estimate_t *estimate = salloc(sizeof *estimate);
		estimate->tdoa_01 = g_ptr_array_index(tdoa_results[0],i0);
		estimate->tdoa_12 = g_ptr_array_index(tdoa_results[1],i1);
		estimate->tdoa_20 = g_ptr_array_index(tdoa_results[2],i2);
		double tdoa_01 = estimate->tdoa_01->tdoa;
		double tdoa_12 = estimate->tdoa_12->tdoa;
		double tdoa_20 = estimate->tdoa_20->tdoa;
		locate_point(tdoa_01,tdoa_12,tdoa_20,station_earthpos,&estimate->location,estimate->heuristics);
		g_ptr_array_add(estimates,estimate);
	}


	/*===========================================================
	 * now we discard or select according to any of the criteria 
	 * we have.
	 *==========================================================*/
	estimate_t *best_estimate = salloc(sizeof(estimate_t));
	
	// e.g.: select_single(estimates,LOCATION_CONSISTENCY_HEUR,POSITION_FROM_BOTTOM,0);
	// or to take the median location spatially:
	best_estimate->location.lat_deg = value(estimates,LOCATION_LATDEG_HEUR,POSITION_FROM_MIDDLE,0);
	best_estimate->location.lat_min = value(estimates,LOCATION_LATMIN_HEUR,POSITION_FROM_MIDDLE,0);
	best_estimate->location.lng_deg = value(estimates,LOCATION_LNGDEG_HEUR,POSITION_FROM_MIDDLE,0);
	best_estimate->location.lng_min = value(estimates,LOCATION_LNGMIN_HEUR,POSITION_FROM_MIDDLE,0);

	/*===========================================================
	 * give an uncertainty to the point(s) we are returning
	 *==========================================================*/

	/*==========================================================
	 * final cleanup
	 *==========================================================*/

	for (int i=0; i<NUM_STATIONS; i++)
	{
		g_ptr_array_foreach(channels[i],(GFunc)free,NULL);
		g_ptr_array_free(channels[i],TRUE); 
		g_ptr_array_foreach(tdoa_results[i],(GFunc)free,NULL);
		g_ptr_array_free(tdoa_results[i],TRUE);
	}
	g_ptr_array_foreach(estimates,(GFunc)free,NULL);
	g_ptr_array_free(estimates,TRUE);


	return best_estimate;

#if 0
	
	//back when the days were simpler, after we'd grabbed the waveforms 
	//we'd just arbitrarily pick a channel and go for it:

	double *filtered[NUM_STATIONS];
	for (int i=0; i<NUM_STATIONS; i++)
	{
		filtered[i] = filter(waveforms[i][1],nsamples,3000,5000);
	}


	double tdoa_01,tdoa_12,tdoa_20;
	double heur[1024];
	int method = GCC_PLAIN;
	tdoa_01 = time_difference_of_arrival(filtered[0],filtered[1],nsamples,method,heur);
	tdoa_12 = time_difference_of_arrival(filtered[1],filtered[2],nsamples,method,heur);
	tdoa_20 = time_difference_of_arrival(filtered[2],filtered[0],nsamples,method,heur);
	earthpos_t station_earthpos[NUM_STATIONS];
	load_station_positions(station_earthpos);
	earthpos_t location;
	locate_point(tdoa_01,tdoa_12,tdoa_20,station_earthpos,&location,heur);

	printf("Error on earth = %lf\n",earth_distance(current,&location));
	if (current)
	{
		printf("Error: %lf\n",earth_distance(&location,current));
	}
#endif

}




void load_config(void)
{
	base_dir = param_get_string(LOCALIZATION_PARAM_GROUP,"base_dir");
	date_dir = param_get_string(LOCALIZATION_PARAM_GROUP,"date_dir");
	station_dir[0] = param_get_string(LOCALIZATION_PARAM_GROUP,"station0_dir");
	station_dir[1] = param_get_string(LOCALIZATION_PARAM_GROUP,"station1_dir");
	station_dir[2] = param_get_string(LOCALIZATION_PARAM_GROUP,"station2_dir");
	BREATHING_SPACE = param_get_double(LOCALIZATION_PARAM_GROUP,"breathing_space");
	kml_file    = param_get_string(LOCALIZATION_PARAM_GROUP,"kml_file");
	result_file = param_get_string(LOCALIZATION_PARAM_GROUP,"result_file");
	click_threshold = param_get_double(LOCALIZATION_PARAM_GROUP,"click_threshold");
	compress_clicktracks = param_get_integer(LOCALIZATION_PARAM_GROUP,"compress_clicktracks");
	
	dprintf(5,"Loaded configuration.\n");
}

/* just for testing */
void analyze_channel_noise(channel_t *chan)
{
	chan->heuristics[CHANNEL_NOISE_HEUR] = abs(chan->channel - 1);
}

/* processes a whole bunch of regions which are specified in the given file */
void process_file(char *filename)
{
	dp(21, "process_file(%s)\n", filename);
	FILE *fp = fopen(filename,"r");
	if (!fp)
	{
		printf("Failed to open the file '%s'.\n",filename);
		return;
	}

	double start_in_seconds;
	double length_in_seconds;

	/* known positions are declared by name (a string) in the file. this hashtable
	 * maps these names to the actual (earthpos_t*) */
	GHashTable* known_positions = g_hash_table_new_full(g_str_hash,g_str_equal,free,free);
	
	/* sometimes it's useful to plot a heuristic versus error over all the calls that
	 * we process. */
	GArray *data_to_plot = g_array_new(FALSE,FALSE,sizeof(point2d_t));

	GPtrArray *estimates = g_ptr_array_new();

	/* see the sample batch file for it's format */
	char line[MAX_PATH_LEN];
	while (fgets(line,MAX_PATH_LEN,fp))
	{
		char timestr[16];
		if (strlen(line) <2)
			continue;
		if (line[0] == '#')
		{
			if (line[1] == '@')
			{
				/* this line defines a new known position or declares that we are using one */
				char name[64];
				double lat_deg, lat_min, lng_deg, lng_min;
				if (sscanf(line,"#@%s =%lf %lf, %lf %lf",name,&lat_deg,&lat_min,&lng_deg, &lng_min)==5)
				{
					dprintf(10,"Known position: '%s'\n",name);
					earthpos_t *pos = salloc(sizeof(earthpos_t));
					set_earthpos(pos,lat_deg,lat_min,lng_deg,lng_min);
					g_hash_table_insert(known_positions,g_strdup(name),pos);
				}
				else
				{
					dprintf(5,"Currently using known position: '%s'\n",name);
					current = g_hash_table_lookup(known_positions,name);
				}
			}
		}
		else if (sscanf(line,"%s ; %lf",(char *)&timestr,&length_in_seconds) == 2)
		{
			/* this line contains a region which should be processed */
			start_in_seconds = parse_time(timestr);
			estimate_t *result = analyze_region(start_in_seconds,length_in_seconds);
			if (current != NULL)
			{
				double error = earth_distance(&result->location,current);
				dprintf(1,"Error is %.1lf metres\n",error);
				point2d_t p;
				p.x = result->uncertainty;
				p.y = error;
				g_array_append_val(data_to_plot,p);
			}
			result->start_time = start_in_seconds;
			result->length = length_in_seconds;
			write_result(start_in_seconds,length_in_seconds,&result->location,result->uncertainty);
			g_ptr_array_add(estimates,result);
			//free(result);
		}
	}


	/* plot the heuristic data */
	if (graphing >= 10)
	{
		pyplotpointdata((point2d_t *)data_to_plot->data,data_to_plot->len,BLOCKING);
	}

	/* we reall need to write them all together, because
	 * of the closing tags of xml file */
	/* i.e. annoying to do them one by one. */
	earthpos_t station_earthpos[NUM_STATIONS];
	load_station_positions(station_earthpos);
	write_kml_file(station_earthpos,known_positions,estimates);

	g_ptr_array_foreach(estimates,(GFunc)free,NULL);
	g_ptr_array_free(estimates,TRUE);
	g_hash_table_destroy(known_positions);
	g_array_free(data_to_plot,TRUE);
	fclose(fp);
}




int main(int argc, char **argv) {
	char *usage = "<start time> <length in seconds> OR <filename>\n";
	graphing  = 0;
	simple_option_parsing(argc, argv, usage);

	if (argc-optind < 1  || argc-optind > 2)
		die("%s", usage);
	
	/* these must be called before most other things */
	load_config();
	dataman_scan();

	if (argc-optind == 1)
	{
		process_file(argv[optind]);
	}
	else // argc == 3 
	{
		double start_in_seconds	  = parse_time(argv[optind]);
		double length_in_seconds  = atof(argv[optind+1]);

		analyze_region(start_in_seconds,length_in_seconds);
	}

	dataman_cleanup();

	return 0;
}

// END

#if 0
/* example of analyzing the filtered waveforms */
void test_analyze_channel(channel_t *chan)
{
	chan->heuristics[CHANNEL_NOISE_HEUR] = fabs(chan->channel - 1);
}

int select(GPtrArray *array[NUM_STATIONS], int  heur_id, int query_type, int threshold)
{
	for (int i=0; i<NUM_STATIONS; i++)
	{
		sort_by_heuristic(array[i],heur_id);
		switch (query_type)
		{
		case VALUE_ABOVE:
		case VALUE_BELOW:
			select_value(array[i],heur_id,query_type,threshold);
			break;
		case POSITION_FROM_TOP:
		case POSITION_FROM_MIDDLE:
		case POSITION_FROM_BOTTOM:
			select_position(array[i],heur_id,query_type,threshold);
			break;
		case PERCENT_FROM_TOP:
		case PERCENT_FROM_MIDDLE:
		case PERCENT_FROM_BOTTOM:
			select_percentage(array[i],heur_id,query_type,threshold);
			break;
		}
	}
}

/* analyzes the specified region and returns the estimated location as `bestpoint_on_earth' */
double analyze_regionCURRENT(double start_in_seconds, double length_in_seconds, earthpos_t *bestpoint_on_earth)
{
	dprintf(1,
"======================================================\n\
 Analyzing region of length %.1lf starting at %.1lf\n\
======================================================\n",length_in_seconds,start_in_seconds);

	/* we will actually grab a slightly larger region than the one
	 * specified */
	start_in_seconds  = start_in_seconds - BREATHING_SPACE;
	length_in_seconds = length_in_seconds + 2*BREATHING_SPACE;

	double *waveforms[NUM_STATIONS][NUM_CHANNELS];
	for (int i=0; i<NUM_STATIONS; i++)
	{
		grab_station_waveforms(waveforms[i],i,start_in_seconds,length_in_seconds);
	}
	dprintf(5,"Successfully grabbed all waveforms.\n");

	index_t nsamples = length_in_seconds * SAMPLING_RATE;

	
	

	double *filtered[NUM_STATIONS][NUM_CHANNELS];
	for (int i=0; i<NUM_STATIONS; i++)
	{
		for (int j=0; j<NUM_CHANNELS; j++)
		{
			filtered[i][j] = filter(waveforms[i][j],nsamples,3000,5000);
		}
	}
	dprintf(5,"Filtered the waveforms.\n");

	{
	double TDOA_01[NUM_CHANNELS][NUM_CHANNELS];
	double TDOA_12[NUM_CHANNELS][NUM_CHANNELS];
	double TDOA_20[NUM_CHANNELS][NUM_CHANNELS];
	double heuristics[3][NUM_CHANNELS][NUM_CHANNELS];

	double HEURISTICS[NUM_STATIONS][NUM_CHANNELS];
	for (int i=0; i<NUM_STATIONS; i++)
	for (int j=0; j<NUM_CHANNELS; j++)
	{
		HEURISTICS[i][j] = 0;
	}

	double bestheur_01 = 1000000;
	double bestheur_12 = 1000000;
	double bestheur_20 = 1000000;
	double besttdoa_01,besttdoa_12,besttdoa_20;
	int method = GCC_PLAIN;
	for (int i=0; i<NUM_CHANNELS; i++)
	for (int j=0; j<NUM_CHANNELS; j++)
	{
		graphing = 0;
		TDOA_01[i][j] = ((double)time_difference_of_arrival(filtered[0][i],filtered[1][j],nsamples,method,&heuristics[0][i][j]))/SAMPLING_RATE;
		TDOA_12[i][j] = ((double)time_difference_of_arrival(filtered[1][i],filtered[2][j],nsamples,method,&heuristics[1][i][j]))/SAMPLING_RATE;
		TDOA_20[i][j] = ((double)time_difference_of_arrival(filtered[2][i],filtered[0][j],nsamples,method,&heuristics[2][i][j]))/SAMPLING_RATE;

		if (heuristics[0][i][j] < bestheur_01)
		{
			bestheur_01 = heuristics[0][i][j];
			besttdoa_01 = TDOA_01[i][j];
		}
		if (heuristics[1][i][j] < bestheur_12)
		{
			bestheur_12 = heuristics[1][i][j];
			besttdoa_12 = TDOA_12[i][j];
		}
		if (heuristics[2][i][j] < bestheur_20)
		{
			bestheur_20 = heuristics[2][i][j];
			besttdoa_20 = TDOA_20[i][j];
		}

		HEURISTICS[0][i] += heuristics[0][i][j];
		HEURISTICS[1][j] += heuristics[0][i][j];
		HEURISTICS[1][i] += heuristics[1][i][j];
		HEURISTICS[2][j] += heuristics[1][i][j];
		HEURISTICS[2][i] += heuristics[2][i][j];
		HEURISTICS[0][j] += heuristics[2][i][j];
	}

	GArray *valchan[NUM_STATIONS];
	valchan[0] = g_array_new(FALSE,FALSE,sizeof(int));
	valchan[1] = g_array_new(FALSE,FALSE,sizeof(int));
	valchan[2] = g_array_new(FALSE,FALSE,sizeof(int));
	int bestchan[NUM_STATIONS];
	for (int i=0; i<NUM_STATIONS; i++)
	{
		dprintf(0,"Heur station %d: ",i);
		for (int j=0; j<NUM_CHANNELS; j++)
		{
			HEURISTICS[i][j] /= 32;
			dprintf(0," %g ",HEURISTICS[i][j]);
		}

		int maxheur = max(HEURISTICS[i],NUM_CHANNELS);
		int minheur = min(HEURISTICS[i],NUM_CHANNELS);
		double cutoff = 0.5*(maxheur+minheur);
		for (int j=0; j<NUM_CHANNELS; j++)
		{
			if (HEURISTICS[i][j] < cutoff)
			{
				g_array_append_val(valchan[i],j);
			}
		}
		dprintf(0,"\n");

		minwithindex(HEURISTICS[i],NUM_CHANNELS,&bestchan[i]);
		
		dprintf(0,"Station %d has %d valid channels\n",i,valchan[i]->len);	
		if (valchan[i]->len == 0)
		{
			g_array_append_val(valchan[i],bestchan[i]);
		}
	}
	GArray *TDOA_01_culled = g_array_new(FALSE,FALSE,sizeof(double));
	GArray *TDOA_12_culled = g_array_new(FALSE,FALSE,sizeof(double));
	GArray *TDOA_20_culled = g_array_new(FALSE,FALSE,sizeof(double));
	
	for (int i=0; i<valchan[0]->len; i++)
	for (int j=0; j<valchan[1]->len; j++)
		g_array_append_val(TDOA_01_culled,
				   TDOA_01[g_array_index(valchan[0],int,i)][g_array_index(valchan[1],int,j)]);

	for (int i=0; i<valchan[1]->len; i++)
	for (int j=0; j<valchan[2]->len; j++)
		g_array_append_val(TDOA_12_culled,
				   TDOA_12[g_array_index(valchan[1],int,i)][g_array_index(valchan[2],int,j)]);

	for (int i=0; i<valchan[2]->len; i++)
	for (int j=0; j<valchan[0]->len; j++)
		g_array_append_val(TDOA_20_culled,
				   TDOA_20[g_array_index(valchan[2],int,i)][g_array_index(valchan[0],int,j)]);


	g_array_sort(TDOA_01_culled,cmp_double);
	g_array_sort(TDOA_12_culled,cmp_double);
	g_array_sort(TDOA_20_culled,cmp_double);
/*	double tdoa_01 = median((double *)TDOA_01_culled->data,TDOA_01_culled->len);
	double tdoa_12 = median((double *)TDOA_12_culled->data,TDOA_12_culled->len);
	double tdoa_20 = median((double *)TDOA_20_culled->data,TDOA_20_culled->len);
//	double tdoa_01 = TDOA_01[bestchan[0]][bestchan[1]];
//	double tdoa_12 = TDOA_12[bestchan[1]][bestchan[2]];
//	double tdoa_20 = TDOA_20[bestchan[2]][bestchan[0]];
//	tdoa_01 = besttdoa_01;
//	tdoa_12 = besttdoa_12;
//	tdoa_20 = besttdoa_20;
	{
	earthpos_t station_earthpos[NUM_STATIONS];
	load_station_positions(station_earthpos);
	int consistency = locate_point(tdoa_01,tdoa_12,tdoa_20,station_earthpos,bestpoint_on_earth);
	}
	return 0;	*/
	for (int i=0; i<TDOA_01_culled->len; i++)
	{
		printf("%lf ",g_array_index(TDOA_01_culled,double,i));
	}
	putchar('\n');
	for (int i=0; i<TDOA_12_culled->len; i++)
	{
		printf("%lf ",g_array_index(TDOA_12_culled,double,i));
	}
	putchar('\n');
	for (int i=0; i<TDOA_20_culled->len; i++)
	{
		printf("%lf ",g_array_index(TDOA_20_culled,double,i));
	}
	putchar('\n');

	earthpos_t station_earthpos[NUM_STATIONS];
	load_station_positions(station_earthpos);
//	int consistency = locate_point(tdoa_01,tdoa_12,tdoa_20,station_earthpos,bestpoint_on_earth);

	/* so we have calculated all the TDOAs
	 * used the cross-correlation heuristic to discard a channel or two
	 * using the remaining channels we pinpoint the location for all the TDOAs determined
	 * by those channels.
	 *
	 * now we want to use the consistency to discard the bad ones of these results
	 * and then take median of latitude and longitude of the result.
	 *
	 * so... any consistency < 5 is included.
	 * if taking these doesn't make up at least 1/3 of the points, then we take the next smallest ones
	 *
	 *
	 * so we need to keep an array of all of the estimated locations, along with their
	 * consistency (and error for testing)
	 *
	 * need to sort by consistency.
	 * take those of lowest consistency until they are above 5 and we have met our quota
	 *
	 * then take median of these lat lngs
	 */

	int N = TDOA_01_culled->len*TDOA_12_culled->len*TDOA_20_culled->len;
	result_t results[N];

/*	double consistency[TDOA_01_culled->len][TDOA_12_culled->len][TDOA_20_culled->len];
	double error[TDOA_01_culled->len][TDOA_12_culled->len][TDOA_20_culled->len];
	double ERROR[TDOA_01_culled->len*TDOA_12_culled->len*TDOA_20_culled->len];
	double CONSISTENCY[TDOA_01_culled->len*TDOA_12_culled->len*TDOA_20_culled->len];
*/
	for (int i=0; i<TDOA_01_culled->len; i++)
	for (int j=0; j<TDOA_12_culled->len; j++)
	for (int k=0; k<TDOA_20_culled->len; k++)
	{
		double tdoa_01 = g_array_index(TDOA_01_culled,double,i);
		double tdoa_12 = g_array_index(TDOA_12_culled,double,j);
		double tdoa_20 = g_array_index(TDOA_20_culled,double,k);

		result_t *result = &results[i*TDOA_12_culled->len*TDOA_20_culled->len + j*TDOA_20_culled->len + k];
		result->consistency = locate_point(tdoa_01,tdoa_12,tdoa_20,station_earthpos,&result->location);
		result->error = earth_distance(current,&result->location);
//		printf("Lat min = %.10lf, Lng min = %.10lf\n",result->location.lat_min,result->location.lng_min);

//		consistency[i][j][k] = locate_point(tdoa_01,tdoa_12,tdoa_20,station_earthpos,&res);
//		error[i][j][k] = earth_distance(current,&res);


//		ERROR[i*TDOA_12_culled->len*TDOA_20_culled->len + j*TDOA_20_culled->len + k] = error[i][j][k];
//		CONSISTENCY[i*TDOA_12_culled->len*TDOA_20_culled->len + j*TDOA_20_culled->len + k] = consistency[i][j][k];
	}

//	pyplotrealx(CONSISTENCY,ERROR,TDOA_01_culled->len*TDOA_12_culled->len*TDOA_20_culled->len);

	
	qsort(&results,N,sizeof(result_t),result_compare);

	GArray *bestresults = g_array_new(FALSE,FALSE,sizeof(result_t));
	int num_required = N/3;
	for (int i=0; i<N; i++)
	{
		double consistency = results[i].consistency;
		if ((consistency > 5) && (bestresults->len > num_required))
			break;

		g_array_append_val(bestresults,results[i]);
	}
	dprintf(0,"Have %d results to take the median over (min was %d)\n",bestresults->len,num_required);

	int M = bestresults->len;
	double latdegs[M],latmins[M],lngdegs[M],lngmins[M];
	for (int i=0; i<M; i++)
	{
		latdegs[i] = g_array_index(bestresults,result_t,i).location.lat_deg;
		latmins[i] = g_array_index(bestresults,result_t,i).location.lat_min;
		lngdegs[i] = g_array_index(bestresults,result_t,i).location.lng_deg;
		lngmins[i] = g_array_index(bestresults,result_t,i).location.lng_min;
	}
	qsort(latdegs,M,sizeof(double),cmp_double);
	qsort(latmins,M,sizeof(double),cmp_double);
	qsort(lngdegs,M,sizeof(double),cmp_double);
	qsort(lngmins,M,sizeof(double),cmp_double);
	double latdeg = median(latdegs,M);
	double lngdeg = median(lngdegs,M);
	double latmin = median(latmins,M);
	double lngmin = median(lngmins,M);
	set_earthpos(bestpoint_on_earth,latdeg,latmin,lngdeg,lngmin);
//	pyplotrealx(latmins,lngmins,M-1);
//	pyplotrealx(latmins,lngmins,M);
	for (int i=0; i<M; i++)
	{
		printf("Lat min = %.10lf, Lng min = %.10lf\n",latmins[i],lngmins[i]);
	}
	printf("Correct:\n");
        printf("Lat min = %.10lf, Lng min = %.10lf\n",current->lat_min,current->lng_min);
//	set_earthpos_radians(bestpoint_on_earth,lat,lng);

	return 100.0/bestresults->len;
	}
	/*  */
#endif
