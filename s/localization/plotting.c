#include "i.h"

/* in the future may be able to use a better plotting method such as calling
 * matplotlib from C. this gnuplotf() has issues. */

void plotreal(double *wav, index_t len)
{
	gnuplotf("length=%d title='' %lf",len,wav);
}

void plotrealx(double *xs, double *wav, index_t len)
{
	gnuplotf("length=%d title='' %t %lf %lf",len,xs,wav);
}

void plot2real(double *wav1, index_t len1, double *wav2, index_t len2)
{
	gnuplotf("rows=2 length=%d title='' %lf ; length=%d title='' %lf",len1,wav1,len2,wav2);
}

void plot3real(double *wav1, index_t len1, double *wav2, index_t len2, double *wav3, index_t len3)
{
	gnuplotf("rows=3 length=%d title='' %lf ; length=%d title='' %lf ; length=%d title='' %lf",len1,wav1,len2,wav2,len3,wav3);
}

void plot4real(double *wav1, index_t len1, double *wav2, index_t len2, double *wav3, index_t len3, double *wav4, index_t len4)
{
	gnuplotf("rows=4 length=%d title='' %lf ; length=%d title='' %lf ; length=%d title='' %lf ; length=%d title='' %lf",len1,wav1,len2,wav2,len3,wav3,len4,wav4);
}

/* we plot the absolute value of these complex numbers */
void plotcomplex(fftw_complex *out, index_t nsamples)
{
	double *outabs = (double *)salloc(sizeof(double)*nsamples);
	for (index_t i=0; i<nsamples; i++)
	{
		outabs[i] = cabs(out[i]);
	}
	plotreal(outabs,nsamples);
	free(outabs);
}

void plotcomplexx(double *xs, fftw_complex *out, index_t nsamples)
{
	double *outabs = (double *)salloc(sizeof(double)*nsamples);
	for (index_t i=0; i<nsamples; i++)
	{
		outabs[i] = cabs(out[i]);
	}

	plotrealx(xs,outabs,nsamples);
	free(outabs);
}


void pyplotpointdata(point2d_t *points,int num_points, int blocking)
{
	if (num_points < 1) return;

	FILE *fp = fopen("/tmp/pyplot.points", "w");
	fprintf(fp, "[ ");
	for (int i=0; i<num_points-1; i++)
	{
		fprintf(fp,"(%lf , %lf) , ",points[i].x,points[i].y);
	}
	fprintf(fp,"(%lf , %lf) ]",points[num_points-1].x,points[num_points-1].y);
	fclose(fp);
	if (blocking == BLOCKING)
	{
		if (system("python pyplotpoints.py"))
		{
			printf("python pyplotpoints.py failed");
			exit(1);
		}
	}
	else
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			execlp("python","python","pyplotpoints.py", (char *)0);
			exit(1);
		}
	}
}

void pyplotrealx(double *xs, double *ys, index_t len)
{
	FILE *fpx = fopen("/tmp/pyplot.linex","w");
	FILE *fpy = fopen("/tmp/pyplot.liney","w");
	fprintf(fpx,"[ ");
	fprintf(fpy,"[ ");
	for (int i=0; i<len-1; i++)
	{
		fprintf(fpx,"'%lf', ",xs[i]);
		fprintf(fpy,"'%lf', ",ys[i]);
	}
	fprintf(fpx,"'%lf' ]",xs[len-1]);
	fprintf(fpy,"'%lf' ]",ys[len-1]);
	fclose(fpx);
	fclose(fpy);
	pid_t pid = fork();
	if (pid == 0)
	{
		execlp("python","python","pyplotline.py", (char *)0);
		exit(1);
	}
}
