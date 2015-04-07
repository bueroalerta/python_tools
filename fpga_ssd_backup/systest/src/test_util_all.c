#include <stdio.h>
#include <time.h>
#include "../include/api/ssd_api.h"
//#include "../include/api/ssd_api.c"
#include "../include/test_tools.h"
#include "../include/test_macro.h"

int main(void)
{
	int i,j,TEST_TIMES,WRITE_TIMES;
	BlockId id,next_id,temp_id;
	uint64_t util_flag_1,util_flag,util_flag_2;
	enum SSD_TEST_RET_TYPE ret = SSD_TEST_OK;
	int ssd_flag = SSD_ERROR;
	uint32_t len=BT_INDEX_SIZE;
	char * buff,*buff_r;	
	
	
	const int BUFFSIZE = BT_DATA_SIZE;  //10m
	buff = (char *)malloc(BUFFSIZE * sizeof(char));
	buff_r = (char *)malloc(BUFFSIZE * sizeof(char));	
	if( (NULL == buff)||(NULL == buff_r)) {
		printf("ERROR! can not malloc buff\n");
		ret = SSD_TEST_ERROR;
	}
	
	memset(buff, 0, BUFFSIZE);
	
	SSD_CASE_INFO("04-03","ALL","TEST the SSD_util ");	
	

//return 0,the wrong situation,but when there are no blocks can used???

	SSD_CASE_INFO("04-03","01","OPEN THE SSD DEVICE");
        if ((ssd_flag = SSD_open(0)) != SSD_OK) {
        SSD_RESULT("excepted","actual", "else",SSD_OK,ssd_flag,0 );
        SSD_ERR("04-03","01","OPEN THE SSD DEVICE");
        ret = SSD_OPEN_ERROR;
        } else {
        SSD_SUCCESS("04-03","01","OPEN THE SSD DEVICE");
        }

  
		
	
	SSD_CASE_INFO("04-03","02","random write 44 times "); 
	TEST_TIMES = 1;//rand()%10+1;
	for(j=0;j<TEST_TIMES;j++){

		WRITE_TIMES = 1600*22+rand()%10000;
    		gen_first_id(&id);
		gen_next_id(id,&next_id,1,1);
		id=next_id;
		temp_id=id;
    		for (i=0;i<WRITE_TIMES;i++){
		
				SSD_CYCLE_INFO("write",i );       		
                		len = (id.m_nHigh & 7) == 0 ? BT_INDEX_SIZE : BT_DATA_SIZE;
				write_process(id,buff,len,SSD_OK);
				gen_next_id(id,&next_id,1,1);
				id=next_id;
    		}

		SSD_CASE_INFO("04-03","03","SSD_util ");
        	util_flag_1 = SSD_util(0);//!= SSD_MAX_VIRBLOCKS_PER_CHANNEL*44) 
        	SSD_RESULT("util","else", "else",util_flag_1,0,0 );

		if(738197504000-WRITE_TIMES*10*1024*1024 == util_flag_1){
			SSD_SUCCESS("","","util is right");
			}
		else{
			SSD_ERR("","","util is wrong");
			}


    
		SSD_CASE_INFO("04-03","04","delete the ftw info");
        	ssd_flag= SSD_ftw(0,delete_all_id, 0);
        	if (ssd_flag!= SSD_OK) {
                	ret = SSD_FTW_ERROR;
                	SSD_RESULT("excepted","actual", "else",SSD_OK,ssd_flag,0 );
                	SSD_ERR("04-03","04","do the ftw");

        	}
        	else {
                	SSD_SUCCESS("04-03","04"," do the ftw");

        	}

        
		SSD_CASE_INFO("04-03","05","SSD_util again");
         	if ((util_flag = SSD_util(0)) != 738197504000) {
		
				 SSD_RESULT("excepted","actual", "else",738197504000,util_flag,0 );
                
				 SSD_ERR("04-03","05","SSD_util");         
   		
				 ret = SSD_UTIL_ERROR;
         			} else {
		 
				 SSD_SUCCESS("04-03","05","SSD_util");
		
    	 	}

	} 

		SSD_CASE_INFO("04-03","06","SSD_close");
	 	SSD_close(0);	

	 if (ret == SSD_TEST_OK)

		SSD_CASE_INFO("04-01","ALL","SSD_util pass");	
	 else
		SSD_CASE_INFO("04-01","ALL","SSD_util pass");
	
	 return 0;
}
