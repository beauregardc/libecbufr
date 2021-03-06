/***
Copyright Her Majesty The Queen in Right of Canada, Environment Canada, 2009-2010.
Copyright Sa Majest� la Reine du Chef du Canada, Environnement Canada, 2009-2010.

This file is part of libECBUFR.

    libECBUFR is free software: you can redistribute it and/or modify
    it under the terms of the Lesser GNU General Public License,
    version 3, as published by the Free Software Foundation.

    libECBUFR is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public
    License along with libECBUFR.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <locale.h>

#include "bufr_api.h"

static void my_abort( const char* msg ) {
	fprintf(stderr,"%s\n", msg );
	exit(0);
}

static int set_value( DataSubset* dss, int desc, int start, int value )
	{
	int n = bufr_subset_find_descriptor( dss, desc, start );
	if ( n >= 0)
		{
		BufrDescriptor* bcv = bufr_datasubset_get_descriptor( dss, n );
		if( bcv )
			{
			bufr_descriptor_set_ivalue( bcv, value );
			}
		}
	return n;
	}

static int cmpinstance( void* i, BufrDescriptor* bv )
	{
	int* instance = (int*)i;
	return --(*instance);
	}

static int cmpless( void* i, BufrDescriptor* bv )
	{
	int value = *(int*)i;
	if( bv->value == NULL ) return -1;
	if( bufr_value_is_missing(bv->value) ) return -1;
	return !(bufr_descriptor_get_ivalue(bv) < value);
	}

int main(int argc, char *argv[])
   {
	const char sect2[] = "Hi Yves!";
   BUFR_Tables   *tables=NULL;
	char msgstr[4096];
	ssize_t msglen = 0;
	int i;

   bufr_begin_api();
	bufr_set_verbose( 1 );
	bufr_set_debug( 1 );

	putenv("BUFR_TABLES=../Tables/");

   tables = bufr_create_tables();
   bufr_load_cmc_tables( tables );  

	bufr_set_abort( my_abort );
	bufr_set_debug_file( "test_find.DEBUG" );
	bufr_set_output_file( "test_find.OUTPUT" );

	/* encode a message with a section 2 content...
	 */
	{
		int n;
		BUFR_Message* msg;
		BUFR_Dataset* dts;
		BufrDescValue bdv;
		DataSubset    *dss;
		BUFR_Template* tmpl = bufr_create_template( NULL, 0, tables, 3 );
		assert( tmpl != NULL );
		
		/* hour/minute/second */
		bufr_init_DescValue( &bdv );
		bdv.descriptor = 301013;
		bufr_template_add_DescValue( tmpl, &bdv, 1 );

		/* lat/lon (location code) */
		bufr_init_DescValue( &bdv );
		bdv.descriptor = 301023;
		bufr_template_add_DescValue( tmpl, &bdv, 1 );

		/* lat increment */
		bufr_init_DescValue( &bdv );
		bdv.descriptor = 5012;
		bufr_template_add_DescValue( tmpl, &bdv, 1 );

		/* need replication to make TLC stuff "happen" */
		bufr_init_DescValue( &bdv );
		bdv.descriptor = 101003;
		bufr_template_add_DescValue( tmpl, &bdv, 1 );

		bufr_init_DescValue( &bdv );
		bdv.descriptor = 12001;
		bufr_template_add_DescValue( tmpl, &bdv, 1 );

		bufr_finalize_template( tmpl );

		dts = bufr_create_dataset(tmpl);
		assert(dts != NULL);

		n = bufr_create_datasubset(dts);
		assert( n >= 0 );

		bufr_expand_datasubset(dts,n);

		dss = bufr_get_datasubset( dts, n );
		assert( dss != NULL );

		n = set_value( dss, 4004, 0, 3 );
		n = set_value( dss, 4005, 0, 11 );
		n = set_value( dss, 4006, 0, 20 );
		n = set_value( dss, 5002, 0, 44 );
		n = set_value( dss, 6002, 0, -77 );

		n = set_value( dss, 5012, 0, 10 );

		n = set_value( dss, 12001, 0, 279 );
		n = set_value( dss, 12001, n+1, 291 );
		n = set_value( dss, 12001, n+1, 45 );

		msg = bufr_encode_message(dts,0);
		assert( msg != NULL );

		msglen = bufr_memwrite_message( msgstr, sizeof(msgstr), msg);
		assert( msglen >= 0 );

		bufr_free_message( msg );
		bufr_free_dataset( dts );
		bufr_free_template( tmpl );
	}

	{
		BUFR_Message* msg = NULL;
		assert( bufr_memread_message(msgstr,msglen,&msg) > 0 );

		BUFR_Dataset* dts = bufr_decode_message(msg, tables);
		assert( dts != NULL );

		for( i=0; i<bufr_count_datasubset(dts); i++ )
			{
			DataSubset    *dss;
			BufrDescriptor* bcv;
			int k, n, len;
			int instance;
			const char* s;
			float          fvalues[5];
			int            ivalues[5];
			BufrDescValue  codes[10];

			/* need this to get at qualifier data */
			bufr_expand_datasubset( dts, i );

			dss = bufr_get_datasubset( dts, i );
			assert( dss != NULL );

			/* should match the second instance of 12001 */
			k = 0;
			bufr_set_key_location( &(codes[k++]), 5002, 64 ); 
			bufr_set_key_int32( &(codes[k++]), 12001, NULL, 0 );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n >= 0 );
			bcv = bufr_datasubset_get_descriptor( dss, n );
			assert(bcv != NULL);
			assert( bcv->descriptor == 12001 );
			assert(bufr_descriptor_get_ivalue(bcv)==291);

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );

			/* can't match */
			k = 0;
			bufr_set_key_location( &(codes[k++]), 5002, 94 ); 
			bufr_set_key_int32( &(codes[k++]), 12001, NULL, 0 );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n < 0 );

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );

			/* matches the first instance */
			k = 0;
			bufr_set_key_int32( &(codes[k++]), 12001, NULL, 0 );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n >= 0 );
			bcv = bufr_datasubset_get_descriptor( dss, n );
			assert(bcv != NULL);
			assert( bcv->descriptor == 12001 );
			assert(bufr_descriptor_get_ivalue(bcv)==279);

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );

			/* matches the second instance */
			k = 0;
			ivalues[0] = 291;
			bufr_set_key_int32( &(codes[k++]), 12001, ivalues, 1 );
			bufr_set_key_int32( &(codes[k++]), 12001, NULL, 0 );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n >= 0 );
			bcv = bufr_datasubset_get_descriptor( dss, n );
			assert(bcv != NULL);
			assert( bcv->descriptor == 12001 );
			assert(bufr_descriptor_get_ivalue(bcv)==291);

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );

			/* match third instance of 12001.
			 * This is an example of using completely arbitrary criteria to
			 * check a value. In this case, it's against a counter, but it
			 * could be anything.
			 */
			k = 0;
			instance = 3;
			bufr_set_key_callback( &(codes[k++]), 12001, cmpinstance, &instance );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n >= 0 );
			bcv = bufr_datasubset_get_descriptor( dss, n );
			assert(bcv != NULL);
			assert( bcv->descriptor == 12001 );
			assert(bufr_descriptor_get_ivalue(bcv)==45);

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );

			/* match third instance of 12001 (i.e. first value less than 200) */
			k = 0;
			instance = 200;
			bufr_set_key_callback( &(codes[k++]), 12001, cmpless, &instance );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n >= 0 );
			bcv = bufr_datasubset_get_descriptor( dss, n );
			assert(bcv != NULL);
			assert( bcv->descriptor == 12001 );
			assert(bufr_descriptor_get_ivalue(bcv)==45);

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );

			/* match fails */
			k = 0;
			instance = 10;
			bufr_set_key_callback( &(codes[k++]), 12001, cmpless, &instance );

			n = bufr_subset_find_values( dss, codes, k, 0 );
			assert( n < 0 );

			for ( i = 0; i < k ; i++ ) bufr_vfree_DescValue( &(codes[i]) );
			}
		bufr_free_message( msg );
	}

   bufr_free_tables( tables );
	exit(0);
   }

