# include "ud_image.h"
#include <math.h>

ud_density_unit	ud_img_jpg_get_density_unit(unsigned char density)
{
	if (density == 1) return (UD_DU_JPG_PBINCH);
	if (density == 2) return (UD_DU_JPG_PBCM);
	return (UD_DU_JPG_UNKNOWN);
}

unsigned char	ud_img_get_quant_bit_id(unsigned char id_b)
{
	unsigned char	id = 0;

	if (!id_b) return (0);
	while (!((id_b >> id) & 1)) ++id;
	return (id);
}

void			ud_img_zz_assign(int val[8][8], int zz_pos, int new_val)
{
	int		row[64] = { 0,
						0, 1,
						2, 1, 0,
						0, 1, 2, 3,
						4, 3, 2, 1, 0,
						0, 1, 2, 3, 4, 5,
						6, 5, 4, 3, 2, 1, 0,
						0, 1, 2, 3, 4, 5, 6, 7,
						7, 6, 5, 4, 3, 2, 1,
						2, 3, 4, 5, 6, 7,
						7, 6, 5, 4, 3,
						4, 5, 6, 7,
						7, 6, 5,
						6, 7,
						7};
	int		col[64] = {	0,
						1, 0,
						0, 1, 2,
						3, 2, 1, 0,
						0, 1, 2, 3, 4,
						5, 4, 3, 2, 1, 0,
						0, 1, 2, 3, 4, 5, 6,
						7, 6, 5, 4, 3, 2, 1, 0,
						1, 2, 3, 4, 5, 6, 7,
						7, 6, 5, 4, 3, 2,
						3, 4, 5, 6, 7,
						7, 6, 5, 4,
						5, 6, 7,
						7, 6,
						7};
	val[row[zz_pos]][col[zz_pos]] = new_val;
}

ud_jpg_comp		*ud_img_jpg_assign_huff_tables(ud_jpg_comp *components, unsigned char comp_id, unsigned char dc_hf_id, unsigned char ac_hf_id, ud_jpg *jpg)
{
	while (components->comp_id != comp_id) ++components;
	components->ac_table = jpg->ac_huff_tables[ac_hf_id];
	components->dc_table = jpg->dc_huff_tables[dc_hf_id];
	return (components);
}

void			ud_img_jpg_reset_dc_prev(ud_jpg_comp *components, unsigned char comp_nbr)
{
	for (ud_ut_count i = 0; i < comp_nbr; ++i, ++components)
		components->dc_prev = 0;
}

unsigned char	*ud_img_jpg_read_huff_table(ud_huff *table, char *bit_pos, unsigned char *img, ud_jpg *jpg, unsigned char *huff_val)
{
	while (table->right_1 || table->left_0)
	{
		if (*bit_pos == 7)
		{
			if (*img == 0xff)
			{
				if (*(img + 1) >= 0xd0 && *(img + 1) <= 0xd7)
				{
					ud_img_jpg_reset_dc_prev(jpg->components, jpg->comp_nbr);
					img += 2;
				}
			}
		}
		if ((*img >> *bit_pos) & 1)
			table = table->right_1;
		else
			table = table->left_0;
		if (!*bit_pos)
		{
			*bit_pos = 7;
			++img;
			if (*(img - 1) == 0xff)
			{
				if (!*img)
					img++;
				else
					printf("marker found : %hhx\n", *img);//parse marker etc
			}
		}
		else
			--(*bit_pos);
	}
	*huff_val = table->val;
	return (img);
}

unsigned char	*ud_img_parse_ac_mcu(ud_huff *table, unsigned char *img, char *bit_pos, int channel_val[8][8], ud_jpg *jpg)
{
	short	neg_mask[16] = {0xffff, 0xfffe, 0xfffc, 0xfff8,
							0xfff0, 0xffe0, 0xffc0, 0xff80,
							0xff00, 0xfe00, 0xfc00, 0xf800,
							0xf000, 0xe000, 0xc000, 0x8000};
	size_t	zz_index = 1;
	unsigned char	huff_val;
	while (1)
	{
		img = ud_img_jpg_read_huff_table(table, bit_pos, img, jpg, &huff_val);
		unsigned char zero_run = ((huff_val >> 4) & 0xf);
		unsigned char category = huff_val & 0xf;
		printf("zero run %hhu AC categorie %hhu\n", zero_run, category);
		if (huff_val == 0x00)
		{
			for (; zz_index != 64; ++zz_index)
				ud_img_zz_assign(channel_val, zz_index, 0);
			//exit(1); //il fat continuer a boucler sur les mcu etc
			printf("quantized mat\n");
			for (ud_ut_count i = 0; i < 8; ++i)
			{
				for (ud_ut_count j = 0; j < 8; ++j)
					printf("%-4d ", channel_val[i][j]);
				printf("\n");
			}
			//printf("bit_pos %hhu\n", *bit_pos);
			/*if (*bit_pos != 7)
			{
				while (*bit_pos)
				{
					printf("%c", ((*img >> *bit_pos) & 1) ? '1' : '0');
					--(*bit_pos);
				}
				printf("%c", (*img & 1) ? '1' : '0');
				*bit_pos = 7;
				++img;
			}*/
			return (img);
		}
		while (zero_run--)
			ud_img_zz_assign(channel_val, zz_index++, 0);
		short val = ((*img >> *bit_pos) & 1) ? 0 : neg_mask[category]; //pk ??
		for (int i = category - 1; i >= 0; --i)
		{
		//	printf("%c", ((*img >> *bit_pos) & 1) ? '1': '0');
			if (*bit_pos == 7)
			{
				if (*img == 0xff)
				{
					if (*(img + 1) >= 0xd0 && *(img + 1) <= 0xd7)
					{
						img += 2;
						ud_img_jpg_reset_dc_prev(jpg->components, jpg->comp_nbr);
					}
				}
			}

			if (((*img >> *bit_pos) & 1))
				val |= (1 << i);
			if (!*bit_pos)
			{
				++img;
				*bit_pos = 7;
				if (*(img - 1) == 0xff)
				{
					if (!*img)
						img++;
					else
						printf("marker found : %hhx\n", *img);//parse marker etc
				}
			}
			else
				--(*bit_pos);
		}
		if (val < 0)
			++val;
		//printf("\nindex = %zu\n", zz_index);
		ud_img_zz_assign(channel_val, zz_index++, val);
		if (zz_index == 64)
		{
			printf("quantized mat\n");
			for (ud_ut_count i = 0; i < 8; ++i)
			{
				for (ud_ut_count j = 0; j < 8; ++j)
					printf("%-4d ", channel_val[i][j]);
				printf("\n");
			}
			//printf("bit_pos %hhu\n", *bit_pos);
			return (img);
		}
	}	
	//val = 0xfffe;
	//printf("\ntest = %hd\n", val);
	/*for (int i = 15; i >= 0; --i)
	{
		printf("%c", ((val >> i) & 1) ? '1': '0');
	}*/
	return (img);
}

unsigned char	*ud_img_parse_dc_mcu(ud_huff *table, unsigned char *img, char *bit_pos, int channel_val[8][8], int *dc_prev, ud_jpg *jpg)//, int *check_eob)
{
	short	neg_mask[16] = {0xffff, 0xfffe, 0xfffc, 0xfff8,
							0xfff0, 0xffe0, 0xffc0, 0xff80,
							0xff00, 0xfe00, 0xfc00, 0xf800,
							0xf000, 0xe000, 0xc000, 0x8000};
	//8
	//1111 10    00101111 10
	//printf("first char %hhu scnd %hhu thrd %hhu table addr %p\n", *img, *(img + 1), *(img + 2), table);
	printf("b pos %hhu, %02.2hhx:%02.2hhx\n", *bit_pos ,*img, *(img + 1));
	while (table->right_1 || table->left_0)
	{
		//printf("%c", ((*img >> *bit_pos) & 1) ? '1': '0');
		//char c = ((*img >> *bit_pos) & 1) + '0';
		//write(1, &c, 1);
		if (*bit_pos == 7)
		{
			if (*img == 0xff)
			{
				if (*(img + 1) >= 0xd0 && *(img + 1) <= 0xd7)
				{
					img += 2;
					ud_img_jpg_reset_dc_prev(jpg->components, jpg->comp_nbr);
				}
			}
		}
		if ((*img >> *bit_pos) & 1)
			table = table->right_1;
		else
			table = table->left_0;
		if (!*bit_pos)
		{
			*bit_pos = 7;
			++img;
			if (*(img - 1) == 0xff)
			{
				if (!*img)
					img++;
				else
					printf("marker found : %hhx\n", *img);//parse marker etc
			}
		}
		else
			--(*bit_pos);
	}
	unsigned char category = table->val;
	printf("DC categorie %hhx (zero count : %d)\n", table->val, table->val >> 4);
	short diff_val = ((*img >> *bit_pos) & 1) ? 0 : neg_mask[category]; //pk ??
	/*if (!*bit_pos)
	{
		*bit_pos = 7;
		++img;
	}
	else
		--(*bit_pos);
	*///--category;//a changer
	//if (table->val == 0x00)
	//	*check_eob = 1;
	for (int i = category - 1; i >= 0; --i)
	{
	//	printf("%c", ((*img >> *bit_pos) & 1) ? '1': '0');
		if (*bit_pos == 7)
		{
			if (*img == 0xff)
			{
				if (*(img + 1) >= 0xd0 && *(img + 1) <= 0xd7)
				{
					img += 2;
					ud_img_jpg_reset_dc_prev(jpg->components, jpg->comp_nbr);
				}
			}
		}
		if (((*img >> *bit_pos) & 1))
			diff_val |= (1 << i);
		if (!*bit_pos)
		{
			++img;
			*bit_pos = 7;
			if (*(img - 1) == 0xff)
			{
				if (!*img)
					img++;
				else
					printf("marker found : %hhx\n", *img);//parse marker etc
			}
		}
		else
			--(*bit_pos);
	}
	if (diff_val < 0)
		++diff_val; // bitstream wtf 0 logique
	printf("diff value = %hd dc_prev %d\n", diff_val, *dc_prev);
//	printf("\ndiff value = %hd\n", diff_val);
	
	channel_val[0][0] = *dc_prev + diff_val;
	*dc_prev = channel_val[0][0];
	return (img);
}
/*
double	ud_pow(double x, int pow)
{
	if (!pow)
		return (1);
	if (pow == 1)
		return (x);
	return (x * ud_pow(x, pow - 1));
}

double ud_cos(double x)
{
	double			s = 1.0;
	double			mem = 0;
	unsigned int	i = 2,k = 1;
	unsigned long	f = 1;

	//for (ud_ut_count j = 0; j < 10; ++j)
	//{
		f = f * (i - 1) * i;
		s = s + (ud_pow(x, i) * ud_pow(-1, k)) / f;
		if (s == mem)
			break;
		mem = s;
		i += 2;
		++k;
	//}
	return s;
}
*/
void			ud_img_jpg_dequantize_mat(int channel_val[8][8], unsigned char **quant_mat)
{
	printf("dequantized mat 00 = %d %hhd\n", channel_val[0][0], quant_mat[0][0]);
	for (ud_ut_count i = 0; i < 8; ++i)
	{
		for (ud_ut_count j = 0; j < 8; ++j)
		{
			channel_val[i][j] *= (int)(quant_mat[i][j]);
			printf("%-4d ", channel_val[i][j]);
		}
		printf("\n");
	}
}

void			ud_img_jpg_compute_idct(int channel_val[8][8], int idct_val[8][8])
{
	
	for (ud_ut_count y = 0; y < 8; ++y)
	{
		for (ud_ut_count x = 0; x < 8; ++x)
		{
			double sum = 0.0;
			for (ud_ut_count u = 0; u < 8; ++u)
			{
				for (ud_ut_count v = 0; v < 8; ++v)
				{
					double Cu = u == 0 ? UD_M_1SQRT2 : 1.0;
					double Cv = v == 0 ? UD_M_1SQRT2 : 1.0;
					double Svu = channel_val[v][u];
					double cos_x_param = (2 * x + 1) * u;
					cos_x_param *= UD_M_PI_16;
					double cos_y_param = (2 * y + 1) * v;
					cos_y_param *= UD_M_PI_16;
					sum += Cu * Cv * Svu * cos(cos_x_param) * cos(cos_y_param);
				}
			}
			sum = sum / 4.0 + 128 ;
			idct_val[y][x] = ud_prot_overflow(ud_round(sum));// y, x ??---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
			//printf("sum : %f, idct[%zu][%zu] = %d\n", sum, y, x, idct_val[y][x]);
		}
	
	}
}
void			ud_img_jpg_reverse_downsampling(int **channel, int idct_val[8][8], ud_jpg_comp *comp, ud_jpg *jpg, size_t chan_off)
{
	size_t		step_row, step_col, start_row, start_col, end_row, end_col;
	size_t		chan_row, chan_col;

	step_row = jpg->mcu_height / 8 / comp->ver_sampling;
	step_col = jpg->mcu_width / 8 / comp->hor_sampling;
	start_row = (chan_off * 8 * step_col / jpg->mcu_width) * 8 * step_row;
	start_col = (chan_off * 8 * step_col) % jpg->mcu_width;
	end_row = start_row + 8 * step_row;
	end_col = start_col + 8 * step_col;

	chan_row = start_row;
	printf(" ver sampl/hor %hhu/%hhu step row %zu step col %zu start row %zu start_col %zu\n", comp->ver_sampling, comp->hor_sampling, step_row, step_col, start_row, start_col);
	for (ud_ut_count i_row = 0; i_row < 8; ++i_row, chan_row += step_row)
	{
		chan_col = start_col;
		for (ud_ut_count j_col = 0; j_col < 8; ++j_col, chan_col += step_col)
		{
			for (ud_ut_count i = 0; i < step_row; ++i)
			{
				for (ud_ut_count j = 0; j < step_col; ++j)
				{
					channel[chan_row + i][chan_col + j] = idct_val[i_row][j_col];
				}
			}
		}
	}
}


unsigned char	*ud_img_jpg_parse_mcu(ud_jpg_comp *comp, ud_jpg *jpg, unsigned char *img, char *bit_pos, int **channel, size_t chan_off)
{
	static int	channel_val[8][8];
	static int	idct_val[8][8];

	img = ud_img_parse_dc_mcu(comp->dc_table, img, bit_pos, channel_val, &(comp->dc_prev), jpg);//, &check_eob);
	img = ud_img_parse_ac_mcu(comp->ac_table, img, bit_pos, channel_val, jpg);
	ud_img_jpg_dequantize_mat(channel_val, ((unsigned char ***)jpg->quantization_mat->val)[comp->quant_mat_id]);
	ud_img_jpg_compute_idct(channel_val, idct_val);
	ud_img_jpg_reverse_downsampling(channel, idct_val, comp, jpg, chan_off);
	return (img);
}

unsigned char	*ud_img_jpg_scan_file(unsigned char *img, ud_jpg *jpg)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	printf("Start of Scan marker : seg len %d\n", seg_len);
	unsigned char	comp_nbr = *(img + 2);
	//unsigned int	***mcu;
	ud_mcu			*mcu = NULL;
	ud_mcu			*prev = NULL;
	//ud_jpg_comp		actual_comp;
	ud_jpg_comp		*components = jpg->components;
	ud_jpg_comp		*comp_list[comp_nbr];

	img += 3;
	for (ud_ut_count i = 0; i < comp_nbr; ++i)
	{
		unsigned char	comp_id = *img++;
		unsigned char	dc_hf_id = (*img >> 4);
		unsigned char	ac_hf_id = (*img++ & 0x0f);
		printf("comp id %hhu dc_huff %hhu ac_huff %hhu\n", comp_id, dc_hf_id, ac_hf_id);
		comp_list[i] = ud_img_jpg_assign_huff_tables(components, comp_id, dc_hf_id, ac_hf_id, jpg);
	}
	unsigned char	spec_start = *img++;
	unsigned char	spec_end = *img++;
	unsigned char	succ_approx = *img++;

	printf("spectral start %hhu spectral end %hhu successive approximation %hhu (not used )\n", spec_start, spec_end, succ_approx);
	char	bit_pos = 7;
	//int ind = 0;
	size_t			mcu_nbr = jpg->img_width / jpg->mcu_width;
	if (jpg->img_width % jpg->mcu_width)
		++mcu_nbr;
	if (jpg->img_height % jpg->mcu_height)
		mcu_nbr *= (jpg->img_height / jpg->mcu_height + 1);
	else
		mcu_nbr *= (jpg->img_height / jpg->mcu_height);
	//jpg->img_height * jpg->img_width / 64 + 64 - (jpg->img_height * jpg->img_width % 64);
	printf("nb mcu = %zu\n", mcu_nbr);
	
	for (ud_ut_count z = 0; z < mcu_nbr; ++z)
	{
		ud_ut_prot_malloc(mcu = ud_ut_malloc(sizeof(ud_mcu)));
		ud_ut_prot_malloc(mcu->val = ud_ut_malloc(sizeof(int **) * (comp_nbr + 1)));
		if (prev)
			prev->next = mcu;
		else
			jpg->mcu_lst = mcu;
		int		***mcu_val = mcu->val;
		mcu_val[comp_nbr] = NULL;
		mcu->nb = (int)z;
		for (ud_ut_count i = 0; i < comp_nbr; ++i)
		{
			ud_ut_prot_malloc(mcu_val[i] = ud_ut_malloc(sizeof(int *) * (jpg->mcu_height)));
			//mcu_val[i][jpg->mcu_height] = NULL;
			for (ud_ut_count x = 0; x < jpg->mcu_height; ++x)  ud_ut_prot_malloc(mcu_val[i][x] = ud_ut_malloc(sizeof(int) * (jpg->mcu_width)));
			size_t	nb_iter = comp_list[i]->hor_sampling * comp_list[i]->ver_sampling;
			printf("mcu[%zu] parse comp[%zu]\n", z, i);
			//actual_comp = comp_list[i];
			for (ud_ut_count j = 0; j < nb_iter; ++j)
			{
				img = ud_img_jpg_parse_mcu(comp_list[i], jpg, img, &bit_pos, mcu_val[i], j);
			}
			for (int r = 0 ; r < jpg->mcu_height; ++r)
			{
				for (int c = 0 ; c < jpg->mcu_width; ++c)
				{
					printf("%-4d ", mcu_val[i][r][c]);
				}
				printf("\n");
			}

		}
		prev = mcu;
		if (jpg->restart_interval && !((z + 1) % jpg->mcu_by_interval) && bit_pos != 7)
		{
			printf("RESTART !! %02.2hhx %02.2hhx %02.2hhx\n", *img, *(img + 1), *(img + 2));
			bit_pos = 7;
			++img;
		}
	}
	mcu->next = NULL;
	return (bit_pos == 7 ? img : img + 1);
}


size_t			ud_img_get_huff_table_size(unsigned char *img, size_t *val_nbr)
{
	size_t	size = 0;
	size_t	nb_iter = 16;

	while (!*img)
	{
		--img;
		--nb_iter;
	}
	size_t	mult = 0;
	for (ud_ut_count i = 0; i < nb_iter; ++i, --img)
	{
		mult = (mult + 1) / 2 + *img;
		size += mult;
		*val_nbr += *img;
	}
	return (size + 1);
}

unsigned char	ud_img_get_byte_len(unsigned char byte)
{
	char i = 7;
	while (i != -1)
		if (((byte >> i++) & 1))
			return (i + 1);
	return (0);
}

void			ud_img_create_end_huff(ud_huff *table, unsigned char *img)
{
	table->val = *img;
	table->val_len = ud_img_get_byte_len(*img);
	table->right_1 = NULL;
	table->left_0 = NULL;
}

unsigned char	*ud_img_fill_huff_table(ud_huff *table, size_t *table_index, unsigned char *img, unsigned char *stage, size_t *nb_val)
{
	ud_huff	*actual = table + *table_index;

	if (!*stage)
	{
		actual->left_0 = actual + 1;
		++(*table_index);
		img = ud_img_fill_huff_table(table, table_index, img, stage + 1, nb_val);
		++(*table_index);
		actual->right_1 = table + *table_index;
		img = ud_img_fill_huff_table(table, table_index, img, stage + 1, nb_val);
	}
	else if (*stage)
	{
		actual->left_0 = actual + 1;
		++(*table_index);
		ud_img_create_end_huff(actual->left_0, img);
		++img;
		--(*nb_val);
		--(*stage);
		++(*table_index);
		actual->right_1 = table + *table_index;
		if (!*nb_val)
			actual->right_1 = NULL;
		else if (!*stage)
			img = ud_img_fill_huff_table(table, table_index, img, stage + 1, nb_val);
		else
		{
			ud_img_create_end_huff(actual->right_1, img);
			++img;
			--(*nb_val);
			--(*stage);
		}
	}
	return (img);
}

void			ud_img_print_huff_table(ud_huff *table, int encrypt, int len)
{
	if (!table->left_0 && !table->right_1)
	{
		printf("huff code: len(%d)", len);
		for (int i = len - 1; i >= 0; --i)
			printf("%c", ((encrypt >> i) & 1) ? '1' : '0');
		printf("   => val: %02.2hhx\n", table->val);
	}
	else
	{
		if (table->left_0)
			ud_img_print_huff_table(table->left_0, encrypt << 1, len + 1);
		if (table->right_1)
			ud_img_print_huff_table(table->right_1, (encrypt << 1) + 1, len + 1);
	}
}

unsigned char	*ud_img_jpg_parse_huffman_table(unsigned char *img, ud_jpg *jpg)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	printf("Define Huffman Table(s) marker : seg len %d\n", seg_len);
	unsigned short	index = 2;
	img += 2;
	//seg_len -= 2;
	while (index < seg_len)
	{
		//printf("%02.2hhx %02.2hhx %02.2hhx\n", *img, *(img + 1), *(img + 2));
		ud_huff_class	table_class = (*img >> 4) ? UD_HC_AC : UD_HC_DC;
		unsigned char	table_id = (*img & 0x0f); // maybe it is bitwise comparison : 0000 = 0 0001 = 1 0010 = 2 0100 = 3... to verify
		printf("Huff table[%hhd] class : %s\n", table_id, table_class == UD_HC_AC ? "AC" : "DC");
		img += 16;
		size_t			val_nbr = 0;
		size_t			huff_table_size = ud_img_get_huff_table_size(img++, &val_nbr);
		size_t			val_nbr_cpy = val_nbr;
		ud_huff			*table;
		size_t			actual_index = 0;
		//size_t			size_index = *(img - 16);
		unsigned char	*stage = (unsigned char *)ud_str_ndup((char *)(img - 16), 16);
	//	if (stage)
	//		;
	   	if (table_class == UD_HC_AC)
		{
			ud_ut_prot_malloc(jpg->ac_huff_tables[table_id] = ud_ut_malloc(sizeof(ud_huff) * huff_table_size));
			table = jpg->ac_huff_tables[table_id];
		}
		else
		{
			ud_ut_prot_malloc(jpg->dc_huff_tables[table_id] = ud_ut_malloc(sizeof(ud_huff) * huff_table_size));
			table = jpg->dc_huff_tables[table_id];
		}
		// if table != NULL free ??
		printf("size : %zu val _nbr %zu\n", huff_table_size, val_nbr);
		ud_img_fill_huff_table(table, &actual_index, img, stage, &val_nbr);
	
		ud_img_print_huff_table(table, 0, 0);
		
		index += 17;
		img -= 16;
		printf("stage outs bytes : ");
		for (ud_ut_count i = 0; i < 16; ++i, ++img)
			printf("%02.2hhx ", *img);
		printf("\nvalues : ");
		for (ud_ut_count i = 0; i < val_nbr_cpy; ++i, ++img, ++index)
			printf("%02.2hhx ", *img);
		printf("\n");
		//index = seg_len;
	}
	return (img);
}

unsigned char	*ud_img_jpg_dct_ctr(unsigned char *img, ud_jpg *jpg, unsigned char dct_type)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	printf("Start Of Frame marker : seg len %d\ndtc_type : %s\n", seg_len, dct_type == UD_IMG_JPG_SOF_BD ? "Baseline" : (dct_type == UD_IMG_JPG_SOF_PD ? "Progressive" : "UNKNOWN"));
	jpg->data_precision = *(img + 2);
	jpg->img_height = ((((unsigned short)*(img + 3)) << 8) | (unsigned short)*(img + 4));
	jpg->img_width = ((((unsigned short)*(img + 5)) << 8) | (unsigned short)*(img + 6));
	unsigned char	comp_nbr = *(img + 7);
	ud_jpg_comp		*comp_lst;

	jpg->comp_nbr = comp_nbr;
	img += 8;
	ud_ut_prot_malloc(jpg->components = ud_ut_malloc(sizeof(ud_jpg_comp) * comp_nbr));
	comp_lst = jpg->components;
	printf("data precision : %hhu bits\n", jpg->data_precision);
	printf("img size : %hu*%hu pixels\n", jpg->img_width, jpg->img_height);
	printf("component nbr = %hhu:\n", jpg->comp_nbr);
	jpg->mcu_height = 8;
	jpg->mcu_width = 8;
	for (ud_ut_count i = 0; i < comp_nbr; ++i, ++comp_lst)
	{
		comp_lst->comp_id = *img++;
		comp_lst->hor_sampling = ud_img_get_quant_bit_id((*img & 0xf0) >> 4) + 1;
		comp_lst->ver_sampling = ud_img_get_quant_bit_id(*img++ & 0x0f) + 1;
		comp_lst->quant_mat_id = *img++;
		comp_lst->mcu_lst = NULL;
		comp_lst->mcu_first = NULL;
		comp_lst->dc_prev = 0;
		if (comp_lst->hor_sampling * 8 > jpg->mcu_width)
			jpg->mcu_width = comp_lst->hor_sampling * 8;
		if (comp_lst->ver_sampling * 8 > jpg->mcu_height)
			jpg->mcu_height = comp_lst->ver_sampling * 8;
		printf("\tcomp[%hhu] :\n\t\t horizontal sampling : %hhu\n\t\t vertical sampling : %hhu\n\t\t quant mat associed : %hhu\n", comp_lst->comp_id, comp_lst->hor_sampling, comp_lst->ver_sampling, comp_lst->quant_mat_id);
	}
	printf("mcu size : %hux%hu px\n", jpg->mcu_width, jpg->mcu_height);
	return (img);
	
}

unsigned char	*ud_img_parse_zig_zag(char **new_mat, unsigned char *img, size_t size_y, size_t size_x)
{
	size_t		top_left_iter;
	size_t		bottom_right_iter;
	size_t		middle_iter;
	
	int			y_w = -1;
	int			x_w = 1;
	int			x = 0;
	int			y = 0;

	if (size_x > size_y)
	{
		top_left_iter = (2 * size_y * size_y / 2 + size_y) / 2;
		bottom_right_iter = (2 * size_y * size_y / 2 - size_y) / 2;
		middle_iter = size_y * size_x - top_left_iter - bottom_right_iter;
	}
	else
	{
		top_left_iter = (2 * size_x * size_x / 2 + size_x) / 2;
		bottom_right_iter = (2 * size_x * size_x / 2 - size_x) / 2;
		middle_iter = size_y * size_x - top_left_iter - bottom_right_iter;
	}
	for (ud_ut_count i = 0; i < top_left_iter; ++i, ++img)
	{
		if (y == -1)
		{
			++y;
			x_w = -1;
			y_w = 1;
		}
		else if (x == -1)
		{
			++x;
			x_w = 1;
			y_w = -1;
		}
		//printf("new_mat[%d][%d] = %2.2hhx\n", y, x, *img);
		new_mat[y][x] = *img;
		y += y_w;
		x += x_w;
	}
	if (size_x > size_y)
	{
		for (ud_ut_count i = 0; i < middle_iter; ++i, ++img)
		{
			if (y == -1)
			{
				++y;
				x_w = -1;
				y_w = 1;
			}
			else if (y == (int)size_y)
			{
				--y;
				x += 2;
				x_w = 1;
				y_w = -1;
			}
		}
	}
	else
	{
		for (ud_ut_count i = 0; i < middle_iter; ++i, ++img)
		{
			if (x == (int)size_x)
			{
				--x;
				y += 2;
				x_w = -1;
				y_w = 1;
			}
			else if (x == -1)
			{
				++x;
				x_w = 1;
				y_w = -1;
			}
		}
	}
	for (ud_ut_count i = 0; i < bottom_right_iter; ++i, ++img)
	{
		if (x == (int)size_x)
		{
			--x;
			y += 2;
			x_w = -1;
			y_w = 1;
		}
		else if (y == (int)size_y)
		{
			--y;
			x += 2;
			x_w = 1;
			y_w = -1;
		}
		//printf("new_mat[%d][%d] = %2.2hhx\n", y, x, *img);
		new_mat[y][x] = *img;
		y += y_w;
		x += x_w;
	}
	return (img);
}

void			ud_img_free_quant_mat(char **mat)
{
	char	**mat_tmp = mat;

	while (*mat_tmp)
		ud_ut_free(*mat_tmp++);
	ud_ut_free(mat);
}

char			**ud_img_create_new_quant_mat(ud_arr **p_quantization_mat, unsigned char nbr_bytes)
{
	char	***t_quantization_mat_val;
	char	**t_quantization_mat_line_val;
	char	*t_quantization_mat_char_val;
	unsigned char	size = nbr_bytes == 64 ? 8 : 16;

	if (!*p_quantization_mat)
		*p_quantization_mat = ud_arr_set(char **, NULL, NULL, NULL, NULL); // max quant_table = 4 
	t_quantization_mat_val = (char ***)(*p_quantization_mat)->val;
	while (*t_quantization_mat_val)
		++t_quantization_mat_val;
	ud_ut_prot_malloc(*t_quantization_mat_val = ud_ut_malloc(sizeof(char **) * (size + 1)));
	t_quantization_mat_line_val = *t_quantization_mat_val;
	for (ud_ut_count i = 0; i < size; ++i, ++t_quantization_mat_line_val)
	{
		ud_ut_prot_malloc(*t_quantization_mat_line_val = ud_ut_malloc(sizeof(char *) * (size + 1)));
		t_quantization_mat_char_val = *t_quantization_mat_line_val;
		for (ud_ut_count j = 0; j < size + 1; ++j, ++t_quantization_mat_char_val) *t_quantization_mat_char_val = '\0';
	}
	*t_quantization_mat_line_val = NULL;
	return (*t_quantization_mat_val);
}

unsigned char	*ud_img_jpg_parse_quantization_mat(unsigned char *img, ud_jpg *jpg)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	printf("Quantization table(s) definition :seg len %d\n", seg_len);
	char	**new_mat;
	img += 2;
	seg_len -= 2;
	while (seg_len)
	{
		unsigned char	nbr_bytes = ((*img >> 4) & 1) ? 128 : 64;
		unsigned char	size_b = ((*img >> 4) & 1) ? 16 : 8;
		unsigned char	mat_id = (*img & 0xf) % 4; // maybe its bitwise comparison : 0000 = 0 0001 = 1 0010 = 2 0100 = 3... to verify
		printf("table[%hhu] : nbr bytes : %hhu\n", mat_id, nbr_bytes);
		new_mat = ud_img_create_new_quant_mat(&(jpg->quantization_mat), nbr_bytes);
		//char	***quant_mat_list = (char ***)jpg->quantization_mat->val;
		++img;
		img = ud_img_parse_zig_zag(new_mat, img, (size_t)size_b, (size_t)size_b);
		seg_len -= (nbr_bytes + 1);

		for (ud_ut_count i = 0; i < nbr_bytes;)
		{
			printf("%02.2hhx ", new_mat[i / size_b][i % size_b]);
			if (!(++i % size_b))
				printf("\n");
		}
		//if (quant_mat_list[mat_id])
		//	ud_img_free_quant_mat(quant_mat_list[mat_id]);
		//quant_mat_list[mat_id] = new_mat;
	}
	return (img);
}

unsigned char	*ud_img_jpg_app_ctr(unsigned char *img, ud_jpg *jpg, unsigned char app_ref)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	printf("app %hhd marker %s: seg len %d\n", app_ref, (char *)(img + 2), seg_len);
	
	if (!app_ref)
	{
		jpg->jfif_seg_len = ((((unsigned short)(*img)) << 8) | (unsigned short)*(img + 1));
		jpg->density_unit = (ud_img_jpg_get_density_unit(*(img + 2)));
		jpg->x_pixel_by_unit = ((((unsigned short)(*(img + 10))) << 8) | (unsigned short)*(img + 11));
		jpg->y_pixel_by_unit = ((((unsigned short)(*(img + 12))) << 8) | (unsigned short)*(img + 13));
		jpg->thumbnail_width = *(img + 14);
		jpg->thumbnail_height = *(img + 15);
		printf("x pixel by unit:%d\ny pixel by unit%d\n", jpg->x_pixel_by_unit, jpg->y_pixel_by_unit);
		if (!jpg->thumbnail || !jpg->thumbnail_height)
			jpg->thumbnail = NULL;
		else
			;//parseminiature
		//return (img + 16);
	}
	//else if (app_ref == 1)
	//	return (img);
	return (img + seg_len);
}

unsigned char *ud_img_jpg_skip_com(unsigned char *img)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	printf("Comment marker: \"%.*s\"\n", seg_len - 2, img + 2);
	return (img + seg_len);
}

unsigned char *ud_img_jpg_restart_interval_ctr(unsigned char *img, ud_jpg *jpg)
{
	unsigned short	seg_len = ((((unsigned short)*img) << 8) | (unsigned short)*(img + 1));
	jpg->mcu_by_interval = ((((unsigned short)*(img + 2)) << 8) | (unsigned short)*(img + 3));
	jpg->restart_interval = 1;
	printf("Restart Interval marker: seg_len (should be 4) : %hu\n\t\tmcu_by_interval : %hu\n", seg_len, jpg->mcu_by_interval);
	if (!jpg->mcu_by_interval)
		jpg->restart_interval = 0;
	return (img + seg_len);
}

unsigned char	*ud_img_jpg_end_of_image(unsigned char *img)
{
	printf("End Of Image marker\n");
	return (img);
}

unsigned char	*ud_img_jpg_start_of_image(unsigned char *img)
{
	printf("Start Of Image marker\n");
	return (img);
}

unsigned char	*ud_img_jpg_read_segment(unsigned char *img, ud_jpg *jpg)
{
	if (*img == UD_IMG_JPG_SOI)
		return (ud_img_jpg_start_of_image(img + 1));
	else if (*img == UD_IMG_JPG_SOF_BD)
		return (ud_img_jpg_dct_ctr(img + 1, jpg, UD_IMG_JPG_SOF_BD));
	else if (*img == UD_IMG_JPG_SOF_PD)
		return (ud_img_jpg_dct_ctr(img + 1, jpg, UD_IMG_JPG_SOF_PD));
	else if (*img == UD_IMG_JPG_DHT)
		return (ud_img_jpg_parse_huffman_table(img + 1, jpg));
	else if (*img == UD_IMG_JPG_DQT)
		return (ud_img_jpg_parse_quantization_mat(img + 1, jpg));
	else if (*img == UD_IMG_JPG_DRI)
		return (ud_img_jpg_restart_interval_ctr(img + 1, jpg));
	else if (*img == UD_IMG_JPG_SOS)
		return (ud_img_jpg_scan_file(img + 1, jpg));
	else if (*img >= UD_IMG_JPG_RST_MIN && *img <= UD_IMG_JPG_RST_MAX)
		return (img + 1);
	else if (*img >= UD_IMG_JPG_APP_MIN && *img <= UD_IMG_JPG_APP_MAX)
		return (ud_img_jpg_app_ctr(img + 1, jpg, (*img & 0x0f)));
	else if (*img == UD_IMG_JPG_COM)
		return (ud_img_jpg_skip_com(img + 1));
	else if (*img == UD_IMG_JPG_EOI)
		return (ud_img_jpg_end_of_image(img + 1));
	else
	{
		printf("UNDEFINE SEG:%02.2hhx\n", *img);
		exit(EXIT_FAILURE);
	}
	return (img + 1);
}

ud_jpg		ud_img_init_jpg_struct(void)
{
	ud_jpg jpg;

	jpg.quantization_mat = NULL;
	jpg.thumbnail = NULL;
	for (ud_ut_count i = 0; i < UD_IMG_MAX_HUFFTABLE; ++i)
	{
		jpg.ac_huff_tables[i] = NULL;
		jpg.dc_huff_tables[i] = NULL;
	}
	jpg.mcu_lst = NULL;
	jpg.restart_interval = 0;
	return (jpg);
}

unsigned char	clip_pixel(int val)
{
	if (val > 255) return (255);
	if (val < 0) return (0);
	return ((unsigned char)val);
}

ud_img		*ud_img_jpg_build_image(ud_jpg *jpg)
{
	ud_img				*img;
	ud_mcu				*mcu = jpg->mcu_lst;
	ud_mcu				*save = mcu;
	//ud_img_pix_ycbcr	pixel;
	ud_img_pix_ycbcr	*pixels;

	ud_ut_prot_malloc(img = ud_ut_malloc(sizeof(ud_img)));
	img->color_space = UD_CS_YCBCR;
	img->pixels = ud_arr_init(ud_img_pix_ycbcr, jpg->img_height * jpg->img_width);
	img->width = jpg->img_width;
	img->height = jpg->img_height;
	pixels = (ud_img_pix_ycbcr*)img->pixels->val;
	ud_ut_count	mcu_row = 0;
	while (mcu)
	{
		for (ud_ut_count i = 0; i < img->height; ++i)
		{
			for (ud_ut_count j = 0; j < img->width;)
			{
				for (ud_ut_count mcu_col = 0; mcu_col < jpg->mcu_width && j < img->width; ++mcu_col, ++j, ++pixels)
				{
					pixels->luminance = clip_pixel(mcu->val[0][mcu_row][mcu_col]);
					if (jpg->comp_nbr > 1)
						pixels->chroma_blue = clip_pixel(mcu->val[1][mcu_row][mcu_col]);
					else
						pixels->chroma_blue = 0;
					if (jpg->comp_nbr > 2)
						pixels->chroma_red = clip_pixel(mcu->val[2][mcu_row][mcu_col]);
					else
						pixels->chroma_red = 0;
				}
				if (j == img->width)
				{
					++mcu_row;
					if (mcu_row == jpg->mcu_height || i + 1 == jpg->img_height)
					{
						mcu = mcu->next;
						save = mcu;
						mcu_row = 0;
					}
					else
						mcu = save;
				}
				else
					mcu = mcu->next;
			}
		
		}
	}
	img->comp_nbr = jpg->comp_nbr;
	return (img);
}

ud_img		*ud_img_decryption_jpg_to_rgb(unsigned char *img_str)
{
	ud_jpg	jpg;
	ud_img	*img;

	jpg = ud_img_init_jpg_struct();
	while (ud_img_jpg_check_marker_start(*img_str))
	{
		unsigned char	*img_ptr = img_str;
		//++img_str;
		img_str = ud_img_jpg_read_segment(img_str + 1, &jpg);
		if (*(img_ptr + 1) == 0xda)
			printf("size of frame %ld\n", img_str - img_ptr);
	}
	img = ud_img_jpg_build_image(&jpg);
	mlx_print_this_shit(img);
	return (img);
}
