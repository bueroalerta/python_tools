#include <pthread.h>
#include <stdlib.h>
#include "ssd_api.h"

#define   TABLESIZE (SSD_MAX_PHYBLOCKS_PER_CHANNEL * 2)
#define   erase_flag 1

struct BlockId ch_ID[44][2000];
int ch_ID_logic[44][2000];
int ch_IDnum[44] = {0};

int check_pass = 1;

/*for crc*/
unsigned long long table[256];
unsigned long long key;

#define CRC_DATA 0x1LL
#define CRC_POLY 0x11edc6f41LL
#define CRC_DATA_BIT (1024 * 8)
#define CRC_POLY_BIT 33
#define CRC_BLOCK_HEADER  32

/* �ַ�ת��ID */
struct BlockId char2id(const char *str)
{
	struct BlockId id;
	int i;
	char ch;
	unsigned long long sum = 0;

	for(i = 0; i < 32; i++) {
		if(16 == i) {
			id.m_nHigh = sum;
			sum = 0;
		}
		ch = *(str + i);
		if(ch >= '0' && ch <= '9')
			sum = sum * 16 + ch - '0';
		else if(ch >= 'A' && ch <= 'F')
			sum = sum * 16 + ch - 'A' + 10;
		else
			sum = sum * 16 + ch - 'a' + 10;
	}

	id.m_nLow = sum;

	return id;
}

/*  ��SSD_ftwʱ����ÿ��ID�鵽��Ӧͨ����ID�����У�������  */
int ID_to_channel(const char *fid, const struct stat *st, int flag)
{
    int channel = (int)st->st_rdev;
    int ch_id_num = ch_IDnum[channel];
    ch_ID[channel][ch_id_num] = char2id(fid);
    ch_ID_logic[channel][ch_id_num] = (int)st->st_ino;
    ch_IDnum[channel]++;
    return 0;
}

/* ��ĳ��ͨ��ָ������飬ָ��page��������1��page */ 
int SSD_rd_phy_block(int channel, int phy_block, int offset, char *buf)
{
    struct ssd_rd_phy_param param;
    char ssd_path[SSD_MAX_DEVNAME];
    int local_ssd_fd = -1;
    int ret;

    if(channel < 0 || channel > SSD_MAX_CHANNEL) {
        printf("error channel type!\n");
        ret = SSD_ERROR;
        goto out;
    }

    param.phy_block = (ssd_u16)phy_block;
    param.offset = (ssd_u16)offset;
    param.channel = (ssd_u8)channel;
    param.buf = buf;
    
    snprintf(ssd_path, SSD_MAX_DEVNAME, "%s%d", "/dev/ssd_Baidu", 0);
    local_ssd_fd = open(ssd_path, O_RDWR);
    if (local_ssd_fd < 0) {
        printf("SSD: /dev/ssd_Baidu%d open failed\n", 0);
        ret = SSD_ERR_OEPN_DEV_FAILED;
        goto out;
    }
    
    ret = ioctl(local_ssd_fd, IOCTL_READ_PHY_BLOCK, &param);
    
    close(local_ssd_fd);

out:
    return ret;
}

/* ����ĳ��ͨ����һ������� */
int SSD_erase_phy_block(int channel, int phy_block)
{
    struct ssd_erase_phy_param param;
    char ssd_path[SSD_MAX_DEVNAME];
    int local_ssd_fd = -1;
    int ret;

    if(channel < 0 || channel > SSD_MAX_CHANNEL) {
        printf("error channel type!\n");
        ret = SSD_ERROR;
        goto out;
    }

    param.phy_block = (ssd_u16)phy_block;
    param.channel = (ssd_u8)channel;
    
    snprintf(ssd_path, SSD_MAX_DEVNAME, "%s%d", "/dev/ssd_Baidu", 0);
    local_ssd_fd = open(ssd_path, O_RDWR);
    if (local_ssd_fd < 0) {
        printf("SSD: /dev/ssd_Baidu%d open failed\n", 0);
        ret = SSD_ERR_OEPN_DEV_FAILED;
        goto out;
    }
    
    ret = ioctl(local_ssd_fd, IOCTL_ERASE_PHY_BLOCK, &param);
    
    close(local_ssd_fd);

out:
    return ret;
}

/* ������ĵ�ַӳ�����߲�������µ�fpga��ram�� */

int SSD_update_tables_to_ram(int channel, int flag, char *buf)
{
    struct ssd_up_table_param param;
    char ssd_path[SSD_MAX_DEVNAME];
    int local_ssd_fd = -1;
    int ret;

    if(channel < 0 || channel > SSD_MAX_CHANNEL) {
        printf("error channel type!\n");
        ret = SSD_ERROR;
        goto out;
    }

    param.type = (ssd_u8)flag;
    param.channel = (ssd_u8)channel;
    param.buf = buf;
    
    snprintf(ssd_path, SSD_MAX_DEVNAME, "%s%d", "/dev/ssd_Baidu", 0);
    local_ssd_fd = open(ssd_path, O_RDWR);
    if (local_ssd_fd < 0) {
        printf("SSD: /dev/ssd_Baidu%d open failed\n", 0);
        ret = SSD_ERR_OEPN_DEV_FAILED;
        goto out;
    }
    
    ret = ioctl(local_ssd_fd, IOCTL_UP_TABLE_TORAM, &param);
    
    close(local_ssd_fd);

out:
    return ret;
}

/* ���ӳ־û���Ϣ���õ���ID��Ӧ�������Ƿ���ȷ */
int check_ID_flush(char *ID_buf, int channel)
{
    int i, ret = -1;
    int id_num = ch_IDnum[channel];
    /*for crc*/
	int crc, nlen;
	int crc_calc;
    int data_check = 1;

    /*  check ID data right or not  */
    for(i = 0; i < id_num; i++) {
        ret = SSD_read(0, ch_ID[channel][i], ID_buf, BT_DATA_SIZE, 0);
        if(SSD_OK != ret) {
            check_pass = 0;
            printf("channel %d: read ID %016lx%016lx error %d\n", channel, ch_ID[channel][i].m_nHigh, ch_ID[channel][i].m_nLow, ret);
            data_check = 0;
            if (ret == -9)
	     	    return -1;
            continue;
        }
        crc = *(int *)ID_buf;
		nlen = *(int *)(ID_buf + 4);

        /* ������ID���Ȳ��ᳬ��10M���Ӵ��ж���Ϊ�˱��ⳤ�ȴ����¼���crc��core */
        if(nlen > BT_DATA_SIZE || nlen <= CRC_BLOCK_HEADER) {
            printf("ID %016lx%016lx nlen %d error\n", ch_ID[channel][i].m_nHigh, ch_ID[channel][i].m_nLow, nlen);
            data_check = 0;
            check_pass = 0;
	     	//return -1;
            continue;
        }

        crc_calc = baidu_crc32_fast(ID_buf + CRC_BLOCK_HEADER, 0, nlen - CRC_BLOCK_HEADER, table);
        if(crc != crc_calc) {
            check_pass = 0;
            printf("channel %d: check crc %016lx%016lx error\n", channel, ch_ID[channel][i].m_nHigh, ch_ID[channel][i].m_nLow);
            data_check = 0;
	     	//return -1;
        }
    }

    if(data_check)
        return 0;
    return -1;
}

/* ����hash���Լ���ַӳ����ؽ������������µ�fpga��ram�� */
/* ��Ҫ�����������ƥ����������޸� */
int update_table(char *erase_table, char *erase_table_new, char *addr_table, char *page_buf, int channel)
{
    int ret = -1, i = 0, j = 0;
    int id_num = ch_IDnum[channel];
    int phy_block, logic_block;
    ssd_u16 blk_stat;
    struct erase_entry *erase, *erase_new;
    struct addr_entry *addr, *addr_new;
    int *data_buf;

    /* ����������ַӳ��� */
    ret = SSD_read_erasetable(channel, erase_table);
    if(0 != ret) {
        check_pass = 0;
        printf("channel %d : fail to read erase table[%d]\n", channel, ret);
		return -1;
    }
    erase = (struct erase_entry *)erase_table;

    ret = SSD_read_addrtable(channel, addr_table);
    if(0 != ret) {
        check_pass = 0;
        printf("channel %d : fail to read addr table[%d]\n", channel, ret);
		return -1;
    }
    addr = (struct addr_entry *)addr_table;

    /* ���use_flagΪ1�������������ID���Ƿ�ƥ�䣨5����ϵ�� */
    int use_flag_num=0;
    for(phy_block = 10; phy_block < 8192; phy_block++) {
        if(phy_block > 4095 && phy_block < 4106)
            continue;

        if((erase + phy_block)->use_flag) {
            if(!(erase + phy_block)->bad_flag)
               use_flag_num++;
            else
               printf("channel %d phy_block %d is used but bad\n", channel, phy_block);
        }
    }

    if(id_num * 5 != use_flag_num) {
        printf("channel %d : old erase table does not match\n", channel);
    }

    /* �Ƚ�����������use_flag��ǳ�0 */
    for(phy_block = 10; phy_block < 8192; phy_block++) {
        if(phy_block > 4095 && phy_block < 4106)
            continue;

         (erase + phy_block)->use_flag = 0;
    }

    /* ����ID�͵�ַӳ�������Ӧ���������use_flagΪ1 */
    for(i = 0; i < id_num; i++) {
       logic_block = ch_ID_logic[channel][i];
       for(j = 0; j < 5; j++) {
           phy_block = (addr + logic_block)->pb_addr;
           (erase + phy_block)->use_flag = 1;
           logic_block++;
       }
    }

    /* ���use_flagΪ1�������������ID���Ƿ�ƥ�䣨5����ϵ�� */
    use_flag_num=0;
    for(phy_block = 10; phy_block < 8192; phy_block++) {
        if(phy_block > 4095 && phy_block < 4106)
            continue;

        if((erase + phy_block)->use_flag) {
            if(!(erase + phy_block)->bad_flag)
               use_flag_num++;
            else
               printf("channel %d phy_block %d is used but bad\n", channel, phy_block);
        }
    }

    if(id_num * 5 != use_flag_num) {
        check_pass = 0;
        printf("channel %d : fail to rebuild erase table\n", channel);
		return -1;
    }

    /*  ���use_flagΪ0������飬�����������ݣ���Ҫ���� */
    for(phy_block = 10; phy_block < 8192; phy_block++) {
        /* �־û�������鲻��Ҫ���м�� */
        if(phy_block > 4095 && phy_block < 4106)
            continue;

        if((erase + phy_block)->bad_flag || (erase + phy_block)->use_flag)
            continue; 

        blk_stat = 0;

        /* �������ĵ�һ��page���ж������Ƿ�ȫ��ffffff */
        ret = SSD_rd_phy_block(channel, phy_block, 0, page_buf);
        if((ret & 0x7f) != 0) {
            printf("channel %d : fail to read phy_block %d, ret[%d]\n", channel, phy_block, ret);
            break;
        }

        data_buf = (int *)page_buf;
        int err_num = 0;
        int shift_data;
        for(i = 0; i < SSD_PAGE_SIZE / 4; i++) {
            if(data_buf[i] != -1) {
                blk_stat = 1;
                printf("page_buf in channel %d phy_block %d pos[%d] value[%08x]\n", channel, phy_block, i, page_buf[i]);
                break;
            }
        }

        /* ������ǿ���״̬�������ݲ�ȫ��ffff����Ҫ���� */
        if((erase + phy_block)->use_flag != blk_stat) {
            //printf("channel %d phy_block %d: erase stat is %d but actual is %d\n", channel, phy_block, (erase + phy_block)->use_flag, blk_stat);
       
            if(erase_flag && blk_stat) {
                ret = SSD_erase_phy_block(channel, phy_block);
                if(ret != 0) 
                    printf("channel %d : fail to read phy_block %d, ret[%d]\n", channel, phy_block, ret);
                (erase + phy_block)->erase_times++;
            }
 
            ret = SSD_rd_phy_block(channel, phy_block, 0, page_buf);
            data_buf = (int *)page_buf;
            blk_stat = 0;
            err_num = 0;
            for(i = 0; i < SSD_PAGE_SIZE / 4; i++) {
                if(data_buf[i] != -1) {
                    blk_stat = 1;
                    //printf("page_buf in channel %d phy_block %d pos[%d] value[%08x] after erase\n", channel, phy_block, i, page_buf[i]);
                    for(shift_data = data_buf[i], j = 0; j <32; j++, shift_data>>1) {
                        if(!(shift_data & 0x00000001))
                            err_num++;
                    }
                }
            }
            if(blk_stat && err_num > 128) {
                printf("page_buf in channel %d phy_block %d pos[%d] err_num[%08x] after erase, mark to bad flag\n", channel, phy_block, i, err_num);
                (erase + phy_block)->bad_flag = 1;
            }
        }
    }

    /* ���޸��Ĳ�������µ�Ӳ�� */
    ret = SSD_update_tables_to_ram(channel, TABLE_ERASE_TYPE, erase_table);
    if(SSD_OK != ret) {
        check_pass = 0;
        printf("channel %d : fail to update erase table to ram[%d]\n", channel, ret);
		return -1;
    }

    /* �����������������ݼ�飬ȷ���������Ѿ�ˢ��ȥ�� */
    ret = SSD_read_erasetable(channel, erase_table_new);
    if(0 != ret) {
        check_pass = 0;
        printf("channel %d : fail to read erase table[%d]\n", channel, ret);
		return -1;
    }
 
    ret = memcmp(erase_table_new, erase_table, TABLESIZE);
    int *buf1 = (int *)erase_table;
    int *buf2 = (int *)erase_table_new;
    if(0 != ret) {
        check_pass = 0;
        printf("channel %d : fail to match erase table[%d]\n", channel, ret);
        /*for(i = 0; i < TABLESIZE/4; i++)
            if(buf1[i] != buf2[i]) 
                printf("channel %d: erase_table_new %x vs erase_table %x pos[%d]\n", channel, buf2[i],
                        buf1[i], i);*/
		return -1;
    }

    return 0;
}

int data_hit(int data, int *buf)
{
    int i = 0;
    for(i = 0; i < 5; i++) {
        if(buf[i] == data)
           return 0;
    }

    return -1;
}

int match_index(int *pb_buf, char *addr_table)
{
    //printf("pb_buf data %d %d %d %d %d\n", pb_buf[0], pb_buf[1], pb_buf[2], pb_buf[3], pb_buf[4]);
    struct addr_entry *addr;
    addr = (struct addr_entry *)addr_table;
    int logic_index, i, pb_addr;

    for(logic_index = 0; logic_index < 8000; logic_index = logic_index + 5) {
        for(i = 0; i < 5; i++) {
            pb_addr = (addr + logic_index + i)->pb_addr;
            //printf("logic_index %d i %d pb_addr %d\n", logic_index, i, pb_addr);
            if(data_hit(pb_addr, pb_buf) != 0)
                break;
        }
        if(i == 5)
            return logic_index;
    }

    return -1;
}

/*   д+�����Ĳ��ԣ�ȷ���޸���������ƥ����ֵĶ�дID�������� */
int  write_erase_test(char *wr_buf, char *rd_buf, char *erase_table, char *addr_table, char *erase_table_new, char *addr_table_new, int channel)
{
    /* write & erase id to see erase_table & addr_table normally changed or not */
    int test_id_num = 5, i = 0, j = 0; 
    int ret = -1;
    int crc_wr, crc_rd;
    int skip_check = 0;
    int logic_index, diff_lb_num, diff_pb_num;
    struct BlockId id;
    int pb_buf[5];

    struct erase_entry *erase, *erase_new;
    struct addr_entry *addr, *addr_new;

    for(i = 0; i < BT_DATA_SIZE; i++) {
        wr_buf[i] = random() % 26 + 'A';
    }

    int id_ch;
    for(id_ch = 0; id_ch < 44; id_ch++) {
        if((id_ch % 4) * 11 + id_ch / 4 == channel)
            break;
    }

    //printf("id_ch %d\n", id_ch);
    id.m_nLow = id_ch;

    crc_wr = baidu_crc32_fast(wr_buf, 0 ,BT_DATA_SIZE, table);

    for(i = 0; i < test_id_num; i++) {
        id.m_nLow += SSD_MAX_CHANNEL;
        id.m_nHigh = (rand() << 1) | 1;
        ret = SSD_write(0, id, wr_buf, BT_DATA_SIZE, 0);
        /*  ����ֵ-28��ʾ��id�Ѿ����ڣ�����д����id  */
        if(ret == -28) {
            i--;
            continue;
        }

        if(SSD_OK != ret) {
            printf("test: write id %016lx%016lx in channel %d error %d\n", id.m_nHigh, id.m_nLow, channel, ret);
            skip_check = 1;
            return -1;
        }
 
        /* ���������µĵ�ַӳ�����������Է�����ı仯�Ƿ�����߼� */
        ret = SSD_read_erasetable(channel, erase_table_new);
        if(0 != ret) {
            skip_check = 1;
            printf("test: channel %d fail to read erase table[%d]\n", channel, ret);
        }
 
        ret = SSD_read_addrtable(channel, addr_table_new);
        if(0 != ret) {
            skip_check = 1;
            printf("test: channel %d fail to read addr table[%d]\n", channel, ret);
        }

        /* �������ʧ�ܣ�������������ű� */
        if(!skip_check) {
            erase = (struct erase_entry *)erase_table;
            erase_new = (struct erase_entry *)erase_table_new;
            addr = (struct addr_entry *)addr_table;
            addr_new = (struct addr_entry *)addr_table_new;
            diff_lb_num = 0;
            diff_pb_num = 0;
            logic_index = 0;

            /* �޸ĵĵ�ַӳ���Ӧ����5���������߼���ַ����logic_index��5������������80 ~ 84ӳ�䵽ͬһ��logic_index */
            /* ����仯��2���߼����index��һ�£���81��86����������� */
            /* ��дid���߼�80ӳ�䵽�����a,��Ҳ����дid֮ǰ��������ӳ���ϵ����ˣ�diff_lb_numֻҪ������5�Ϳ���*/
            //printf("channel %d ID %016lx%016lx : \n", channel, id.m_nHigh, id.m_nLow);
            for(j = 0; j < 8192; j++, addr++, addr_new++) {
                if(addr->pb_addr != addr_new->pb_addr) {
                    diff_lb_num++;
                    //printf("%d(old_%d, new_%d)\n", j, addr->pb_addr, addr_new->pb_addr);
                    int new_logic_index = (j / 5) * 5;
                    if(0 == logic_index)
                        logic_index = new_logic_index;
                    else if(new_logic_index != logic_index)
                        skip_check = 1;
                    //printf("logic_index %d\n", logic_index);
                }
            }
            if(diff_lb_num > 5)  {
                printf("test : channel %d write ID %016lx%016lx: diff_lb_num is %d\n", channel, id.m_nHigh, id.m_nLow, diff_lb_num);
                skip_check = 1;
            }
            
            /* �仯�������϶���5������use_flag��0���1 */
            
            for(j = 0; j < 8192; j++, erase++, erase_new++) {
                if(erase->use_flag != erase_new->use_flag) {
                    //printf("channel %d write ID %016lx%016lx: diff_pb_addr is %d\n", channel, id.m_nHigh, id.m_nLow, j);
                    if(diff_pb_num < 5)
                        pb_buf[diff_pb_num] = j;
                    diff_pb_num++;
                }
            }
            if(diff_pb_num != 5) {
                skip_check = 1;
                printf("test: channel %d write ID %016lx%016lx: diff_pb_num is %d\n", channel, id.m_nHigh, id.m_nLow, diff_pb_num);
            }

            /* ���diff_lb_numΪ0������ַ��պ�û�б仯����Ҫ�����ҳ�logic_index */
            if(diff_lb_num == 0) {
                logic_index = match_index(pb_buf, addr_table_new);
                //printf("match_index ret is %d\n", logic_index);
            }

            erase = (struct erase_entry *)erase_table;
            erase_new = (struct erase_entry *)erase_table_new;
            addr = (struct addr_entry *)addr_table;
            addr_new = (struct addr_entry *)addr_table_new;
            /* ���仯��������Ƿ���5���߼����Ӧ������ */
            for(j = 0; j < 5; j++) {
                int pb_addr = (addr_new + logic_index + j)->pb_addr;
                if((erase + pb_addr)->use_flag || !(erase_new + pb_addr)->use_flag) {
                    skip_check = 1;
                    printf("test: channel %d write ID %016lx%016lx: pb_addr %d old %d, old %d \n", 
                            channel, id.m_nHigh, id.m_nLow, pb_addr, (erase + pb_addr)->use_flag, (erase_new + pb_addr)->use_flag);
                }
            }
        }

        /* ����仯��ȷ�����id,������crc�Ƿ�ƥ�� */
        memset(rd_buf, 0, BT_DATA_SIZE);
        if(!skip_check) {
	        ret = SSD_read(0, id, rd_buf, BT_DATA_SIZE, 0);
		    if(SSD_OK != ret) {
			    printf("test: channel %d ssd read error %016lX%016lX %d\n", channel, id.m_nHigh, id.m_nLow, ret);
			    skip_check = 1;
		    }

	        crc_rd = baidu_crc32_fast(rd_buf, 0, BT_DATA_SIZE, table);
		    if(crc_wr != crc_rd) {
			    skip_check = 1;
			    printf("test: channel %d check error %d vs %d in %016lX%016lX\n", channel, crc_wr, crc_rd, id.m_nHigh, id.m_nLow);
		    }
        }

        /* �����Ƿ�ͨ��ǰ��Ĳ��ԣ�����Ҫɾ����id*/
        ret = SSD_delete(0, id);
        if(0 != ret) {
            skip_check = 1;
            printf("test: channel %d fail to delete id %016lx%016lx, ret %d \n", channel, id.m_nHigh, id.m_nLow, ret);
        }

        if(skip_check)
            return -1;
           
        /* �����ű�����һ��дIDǰ�ľɱ� */
        ret = SSD_read_erasetable(channel, erase_table);
        if(0 != ret) {
            printf("channel %d : fail to read erase table[%d]\n", channel, ret);
            return -1;
        }
 
        ret = SSD_read_addrtable(channel, addr_table);
        if(0 != ret) {
            printf("channel %d : fail to read addr table[%d]\n", channel, ret);
            return -1;
        }

        /* ���ɾ��ID�󣬲�����ı仯�Ƿ����Ԥ�ڣ���Ҫע���¾ɱ���л� */
        diff_pb_num = 0;
        erase = (struct erase_entry *)erase_table_new;
        erase_new = (struct erase_entry *)erase_table;
        for(j = 0; j < 8192; j++, erase++, erase_new++) {
            if((erase->use_flag != erase_new->use_flag) || (erase->erase_times != erase_new->erase_times)) {
                //printf("channel %d erase ID %016lx%016lx: diff_pb in %d\n", channel, id.m_nHigh, id.m_nLow, j);
                diff_pb_num++;
            }
        }
        if(diff_pb_num != 5) {
            printf("test: channel %d erase ID %016lx%016lx: diff_pb_num is %d\n", channel, id.m_nHigh, id.m_nLow, diff_pb_num);
            return -1;
        }

        erase = (struct erase_entry *)erase_table_new;
        erase_new = (struct erase_entry *)erase_table;
        for(j = 0; j < 5; j++) {
            int pb_addr = (addr_new + logic_index + j)->pb_addr;
            if(!(erase + pb_addr)->use_flag || (erase_new + pb_addr)->use_flag) {
                printf("test: channel %d erase ID %016lx%016lx: pb_addr %d old %u, new %u\n", 
                        channel, id.m_nHigh, id.m_nLow, pb_addr, (erase + pb_addr)->use_flag, (erase_new + pb_addr)->use_flag);
                return -1;
            }
        }
    }

    return 0;
}


void *work(void *args)
{
    int channel = (int)args;
    char *erase_table, *erase_table_new;
    char *addr_table, *addr_table_new;
    char *page_buf;
    char *ID_buf, *wr_buf;
    int *data_buf;
    int ret;
    

    erase_table = (char *)malloc(TABLESIZE);
    addr_table = (char *)malloc(TABLESIZE);
    erase_table_new = (char *)malloc(TABLESIZE);
    addr_table_new = (char *)malloc(TABLESIZE);
    page_buf = (char *)malloc(SSD_PAGE_SIZE);
    ID_buf = (char *)malloc(BT_DATA_SIZE);
    wr_buf = (char *)malloc(BT_DATA_SIZE);
	if(!erase_table || !page_buf || !addr_table || !ID_buf || !wr_buf || !erase_table_new || !addr_table_new) {
		printf("channel %d : fail to alloc buffer\n", channel);
        check_pass = 0;
		goto out;
	}

    memset(erase_table, 0, TABLESIZE);
    memset(addr_table, 0, TABLESIZE);
    memset(erase_table_new, 0, TABLESIZE);
    memset(addr_table_new, 0, TABLESIZE);
    memset(page_buf, 0, SSD_PAGE_SIZE);
    memset(ID_buf, 0, BT_DATA_SIZE);

    ret = check_ID_flush(ID_buf, channel);
    if(ret != 0) {
        printf("ID check fail: error IDs\n");
        goto out;
    }

    ret = update_table(erase_table, erase_table_new, addr_table, page_buf, channel);
    if(ret != 0) {
        printf("update_table fail\n");
        goto out;
    }

    ret = write_erase_test(wr_buf, ID_buf, erase_table, addr_table, erase_table_new, addr_table_new, channel);
    if(ret != 0) { 
        printf("write_erase test fail\n");
        check_pass = 0;
    }

out:
    if(erase_table) free(erase_table);
    if(addr_table) free(addr_table);
    if(erase_table_new) free(erase_table_new);
    if(addr_table_new) free(addr_table_new);
    if(page_buf) free(page_buf);
    if(ID_buf) free(ID_buf);
    if(wr_buf) free(wr_buf);
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
       printf("input start_channel, end_channel\n");
       return 1;
    }

    pthread_t id[44];
    int channel=0;
    int ch_start = atoi(argv[1]);
    int ch_end = atoi(argv[2]);

    if(ch_start < 0 || ch_end >= 44) {
        printf("channel error!");
        return 1;
    }

    /*  classify ID into different channel  */
    int ret = SSD_open(0);
    if(SSD_OK != ret) {
        printf("open error: %d\n", ret);
        return 1;
    }

    /*  for crc */
    key = mod2(CRC_DATA, CRC_POLY, CRC_POLY_BIT, CRC_DATA_BIT);
	gen_table(table, key);

    ret = SSD_ftw(0, ID_to_channel, 0);
    if(SSD_OK != ret) {
        printf("ID_to_channel error: %d\n", ret);
        return 1;
    }

    for(channel = ch_start; channel <= ch_end; channel++)
        pthread_create(&id[channel], NULL, work, (void *)channel);

    for(channel = ch_start; channel <= ch_end; channel++)
        pthread_join(id[channel], NULL);

    SSD_close(0);
    if(check_pass) {
        printf("check pass!\n");
        return 0;
    }
 
    return 1;
}


