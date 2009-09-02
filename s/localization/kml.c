#include "i.h"

char *kml_file;
FILE *fp_kml=NULL;
void write_kml(char *name, earthpos_t *A)
{
	if (!fp_kml)
	{
		fp_kml = fopen(kml_file,"w");
		if (!fp_kml)
		{
			printf("Failed to open kmlfile %s\n",kml_file);
			return;
		}
	}
	fprintf(fp_kml,
"<Placemark> \n \
	<name>%s</name> \n \
	<styleUrl>#waypoint</styleUrl> \n \
	<Point> \n \
	<coordinates> \n \
		%.10lf, %.10lf \n \
	</coordinates> \n \
	</Point> \n \
</Placemark>\n\n",name,deg2full(A->lng_deg,A->lng_min),deg2full(A->lat_deg,A->lat_min));
}

const char *xml_header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
const char *kml_namespace = "<kml xmlns=\"http://earth.google.com/kml/2.2\">";
const int spaces_per_level = 2;


void fprintf_tagline(FILE *fp,int level,char *tag,char *format, ...)
{
	int nspaces = level*spaces_per_level;
	for (int i=0; i<nspaces; i++) fputc(' ',fp);

	fprintf(fp,"<%s>",tag);
	va_list ap;
	va_start(ap,format);
	vfprintf(fp,format,ap);	
	fprintf(fp,"</%s>\n",tag);
}
void fputs_tag(int level,char *str,FILE *fp)
{
	int nspaces = level*spaces_per_level;
	for (int i=0; i<nspaces; i++) fputc(' ',fp);
	fputs(str,fp);
	fputc('\n',fp);
}

void write_kml_coordinates(FILE *fp,int level,earthpos_t *point)
{
	fprintf_tagline(fp,level,"coordinates","%.10lf, %.10lf",deg2full(point->lng_deg,point->lng_min),deg2full(point->lat_deg,point->lat_min));
}
char *stationdescriptions[NUM_STATIONS] = {
	"The base station.",
	"The station near the road.",
	"The station in the creek."};

void write_kml_point(FILE *fp,int level,earthpos_t *point,char *name,char *style,char *desc)
{
	fputs_tag(level++,"<Placemark>",fp);
	  fprintf_tagline(fp,level,"name",name);
	  if (desc != NULL) {
	  fprintf_tagline(fp,level,"description",desc);
	  }
	  fprintf_tagline(fp,level,"styleUrl",style);
	  fputs_tag(level++,"<Point>",fp);
	    write_kml_coordinates(fp,level,point);
	  fputs_tag(--level,"</Point>",fp);
	fputs_tag(--level,"</Placemark>",fp);
}

void write_kml_stations(FILE *fp,int level,earthpos_t *stations)
{
	fputs_tag(level++,"<Folder>",fp);
	fprintf_tagline(fp,level,"name","Stations");
	fprintf_tagline(fp,level,"description","Our weathered and beaten tracking stations");
	for (int i=0; i<NUM_STATIONS; i++)
	{
		char name[16]; sprintf(name,"Station %d\n",i);
		write_kml_point(fp,level,&stations[i],name,"#station_style",stationdescriptions[i]);
	}
	fputs_tag(--level,"</Folder>",fp);
}
void write_kml_known(FILE *fp,int level,GHashTable *known)
{
	if (g_hash_table_size(known) == 0)
		return;

	fputs_tag(level++,"<Folder>",fp_kml);
	fprintf_tagline(fp_kml,level,"name","Known Positions");
	fprintf_tagline(fp_kml,level,"description","These are the positions at which we have played playbacks");

	GList *keys = g_hash_table_get_keys(known);
	keys = g_list_sort(keys,(GCompareFunc)strcmp);
	GList *key = g_list_first(keys);
	int count=0;
        while (key)
	{
		char *name = key->data;
		earthpos_t *position = g_hash_table_lookup(known,name);
		char style[32];
		sprintf(style,"#known%d_style",count);
		write_kml_point(fp,level,position,name,style,NULL);
		key = g_list_next(key);
		count = (count+1)%11;
	}
	g_list_free(keys);

	fputs_tag(--level,"</Folder>",fp_kml);
}
void write_kml_estimated(FILE *fp,int level,GPtrArray *estimates)
{
	fputs_tag(level++,"<Folder>",fp_kml);
	fprintf_tagline(fp_kml,level,"name","Estimated Positions");
	fprintf_tagline(fp_kml,level,"description","bleh");

	for (int i=0; i<estimates->len; i++)
	{
		estimate_t *est = g_ptr_array_index(estimates,i);
		char desc[128];
		sprintf(desc,"Region lasting %lf seconds starting at time %lf.\nUncertainty = %lf",est->length,est->start_time,est->uncertainty);
		write_kml_point(fp,level,&est->location,"Estimate bleh","#highlighted_style",desc);
	}
	fputs_tag(--level,"</Folder>",fp_kml);
}
void fprintf_tagsingle(FILE *fp,int level,char *format,...)
{
	va_list ap;
	int nspaces = level*spaces_per_level;
	for (int i=0; i<nspaces; i++) fputc(' ',fp);
	fputc('<',fp);
	va_start(ap,format);
	vfprintf(fp,format,ap);	
	fprintf(fp,">\n");
}
void write_kml_basicstyle(FILE *fp_kml,int level,char *stylename,char *stylelocation)
{
	fprintf_tagsingle(fp_kml,level++,"Style id=\"%s\"",stylename);
	fprintf_tagline(fp_kml,level,"IconStyle","<Icon><href>%s</href></Icon>",stylelocation);
  	fprintf_tagsingle(fp_kml,--level,"/Style");
}
char *bigredstar_location = "http://maps.google.com/mapfiles/kml/shapes/capital_big_highlight.png";
char *red0_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-0.png";
char *red1_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-1.png";
char *red2_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-2.png";
char *red3_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-3.png";
char *red4_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-4.png";
char *red5_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-5.png";
char *red6_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-6.png";
char *red7_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-7.png";
char *red8_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-8.png";
char *red9_location  = "http://www.cse.unsw.edu.au/~berens/icons/red-9.png";
char *red10_location = "http://www.cse.unsw.edu.au/~berens/icons/red-10.png";

void write_kml_file(earthpos_t *stations, GHashTable *known_positions, GPtrArray *estimates)
{
	if (!fp_kml)
	{
		fp_kml = fopen(kml_file,"w");
		if (!fp_kml)
		{
			printf("Failed to open kmlfile %s\n",kml_file);
			return;
		}
	}
	dprintf(5,"Writing kml file...\n");
	char hostname[128];
	gethostname(hostname,128);
	time_t t = time(NULL);
	struct tm* tv = localtime(&t);
	char timestr[128];
	strftime(timestr,128,"%c",tv);
	
fputs(xml_header,fp_kml); fputc('\n',fp_kml);
fputs(kml_namespace,fp_kml); fputc('\n',fp_kml);
int level=1;
fputs_tag(level++,"<Document>",fp_kml);
  fprintf_tagline(fp_kml,level,"name","Ground Parrot Localization (generated by machine '%s' at %s)\n",hostname,timestr);
  fprintf_tagline(fp_kml,level,"open","1");
  fprintf_tagline(fp_kml,level,"description","More information at http://bioacoustics.cse.unsw.edu.au");
  write_kml_basicstyle(fp_kml,level,"station_style",bigredstar_location);
  write_kml_basicstyle(fp_kml,level,"known0_style",red0_location);
  write_kml_basicstyle(fp_kml,level,"known1_style",red1_location);
  write_kml_basicstyle(fp_kml,level,"known2_style",red2_location);
  write_kml_basicstyle(fp_kml,level,"known3_style",red3_location);
  write_kml_basicstyle(fp_kml,level,"known4_style",red4_location);
  write_kml_basicstyle(fp_kml,level,"known5_style",red5_location);
  write_kml_basicstyle(fp_kml,level,"known6_style",red6_location);
  write_kml_basicstyle(fp_kml,level,"known7_style",red7_location);
  write_kml_basicstyle(fp_kml,level,"known8_style",red8_location);
  write_kml_basicstyle(fp_kml,level,"known9_style",red9_location);
  write_kml_basicstyle(fp_kml,level,"known10_style",red10_location);

  fputs_tag(level++,"<Style id=\"highlighted_placemark\">",fp_kml);
    fprintf_tagline(fp_kml,level,"IconStyle","<Icon><href>http://maps.google.com/mapfiles/kml/paddle/red-stars.png</href></Icon>");
  fputs_tag(--level,"</Style>",fp_kml);
  fputs_tag(level++,"<Style id=\"normal_placemark\">",fp_kml);
    fprintf_tagline(fp_kml,level,"IconStyle","<Icon><href>http://maps.google.com/mapfiles/kml/paddle/wht-blank.png</href></Icon>");
  fputs_tag(--level,"</Style>",fp_kml);
  fputs_tag(level++,"<StyleMap id=\"highlighted_style\">",fp_kml);
    fputs_tag(level++,"<Pair>",fp_kml);
      fprintf_tagline(fp_kml,level,"key","normal");
      fprintf_tagline(fp_kml,level,"styleUrl","#normal_placemark",fp_kml);
    fputs_tag(--level,"</Pair>",fp_kml);
    fputs_tag(level++,"<Pair>",fp_kml);
      fprintf_tagline(fp_kml,level,"key","highlight");
      fprintf_tagline(fp_kml,level,"styleUrl","#highlighted_placemark",fp_kml);
    fputs_tag(--level,"</Pair>",fp_kml);
  fputs_tag(--level,"</StyleMap>",fp_kml);
  write_kml_stations(fp_kml,level,stations);
  write_kml_known(fp_kml,level,known_positions);
  write_kml_estimated(fp_kml,level,estimates);
fputs_tag(--level,"</Document>",fp_kml);
fputs_tag(--level,"</kml>",fp_kml);

fclose(fp_kml);
	dprintf(5,"Done writing kml file.\n");

}
