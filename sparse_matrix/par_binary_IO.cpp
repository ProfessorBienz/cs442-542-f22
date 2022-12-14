bool little_endian();

#include "par_binary_IO.hpp"
#include <stdio.h>
#include "limits.h"

bool little_endian()
{
    int num = 1;
    return (*(char *)&num == 1);
}

template <class T>
void endian_swap(T *objp)
{
  unsigned char *memp = reinterpret_cast<unsigned char*>(objp);
  std::reverse(memp, memp + sizeof(T));
}

void readParMatrix(const char* filename, ParMat& A)
{
    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);


    int64_t pos;
    int32_t code;
    int32_t global_num_rows;
    int32_t global_num_cols;
    int32_t global_nnz;
    int32_t idx;
    int n_items_read;
    double val;

    int extra;
    bool is_little_endian = false;

    int ctr, size;

    int sizeof_dbl = sizeof(val);
    int sizeof_int32 = sizeof(code);

    FILE* ifile = fopen(filename, "rb");
    if (fseek(ifile, 0, SEEK_SET)) printf("Error seeking beginning of file\n"); 
    
    n_items_read = fread(&code, sizeof_int32, 1, ifile);
    if (n_items_read == EOF) printf("EOF reading code\n");
    if (ferror(ifile)) printf("Error reading code\n");
    if (code != PETSC_MAT_CODE)
    {
        endian_swap(&code);
        is_little_endian = true;
    }

    n_items_read = fread(&global_num_rows, sizeof_int32, 1, ifile);
    if (n_items_read == EOF) printf("EOF reading code\n");
    if (ferror(ifile)) printf("Error reading N\n");
    n_items_read = fread(&global_num_cols, sizeof_int32, 1, ifile);
    if (n_items_read == EOF) printf("EOF reading code\n");
    if (ferror(ifile)) printf("Error reading M\n");
    n_items_read = fread(&global_nnz, sizeof_int32, 1, ifile);
    if (n_items_read == EOF) printf("EOF reading code\n");
    if (ferror(ifile)) printf("Error reading nnz\n");

    if (is_little_endian)
    {
        endian_swap(&global_num_rows);
        endian_swap(&global_num_cols);
        endian_swap(&global_nnz);
    }

    A.global_rows = global_num_rows;
    A.global_cols = global_num_cols;

    A.local_rows = A.global_rows / num_procs;
    extra = A.global_rows % num_procs;
    A.first_row = A.local_rows * rank;
    if (extra > rank)
    {
        A.local_rows++;
        A.first_row += rank;
    }
    else A.first_row += extra;

    A.local_cols = A.global_cols / num_procs;
    extra = A.global_cols % num_procs;
    A.first_col = A.local_cols * rank;
    if (extra > rank)
    {
        A.local_cols++;
        A.first_col += rank;
    }
    else A.first_col += extra;


    std::vector<int32_t> row_sizes(A.local_rows);
    std::vector<int32_t> col_indices;
    std::vector<double> vals;
    std::vector<int> proc_nnz(num_procs);
    long nnz = 0;

    pos = (4 + A.first_row) * sizeof_int32;
    if (fseek(ifile, pos, SEEK_SET)) printf("Error seeking pos\n"); 
    n_items_read = fread(row_sizes.data(), sizeof_int32, A.local_rows, ifile);
        if (n_items_read == EOF) printf("EOF reading code\n");
        if (ferror(ifile)) printf("Error reading row_size\n");
    for (int i = 0; i < A.local_rows; i++)
    {
        if (is_little_endian) endian_swap(&(row_sizes[i]));
        nnz += row_sizes[i];
    }

    long first_nnz = 0;
    MPI_Exscan(&nnz, &first_nnz, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);

    col_indices.resize(nnz);
    vals.resize(nnz);

    pos = (4 + A.global_rows + first_nnz) * sizeof_int32;
    if (fseek(ifile, pos, SEEK_SET)) printf("Error seeking pos\n"); 
    n_items_read = fread(col_indices.data(), sizeof_int32, nnz, ifile);
        if (n_items_read == EOF) printf("EOF reading code\n");
        if (ferror(ifile)) printf("Error reading col idx\n");
    for (int i = 0; i < nnz; i++)
    {
        if (is_little_endian) endian_swap(&(col_indices[i]));
    }

    pos = (4 + A.global_rows + global_nnz) * sizeof_int32 + (first_nnz * sizeof_dbl);
    if (fseek(ifile, pos, SEEK_SET)) printf("Error seeking pos\n"); 
    n_items_read = fread(vals.data(), sizeof_dbl, nnz, ifile);
        if (n_items_read == EOF) printf("EOF reading code\n");
        if (ferror(ifile)) printf("Error reading value\n");
    for (int i = 0; i < nnz; i++)
    {
        if (is_little_endian) endian_swap(&(vals[i]));
    }
    fclose(ifile);

    int last_col = A.first_col + A.local_cols - 1;
    A.on_proc.rowptr.resize(A.local_rows+1);
    A.off_proc.rowptr.resize(A.local_rows+1);
    A.on_proc.rowptr[0] = 0;
    A.off_proc.rowptr[0] = 0;
    ctr = 0;
    for (int i = 0; i < A.local_rows; i++)
    {
        size = row_sizes[i];
        for (int j = 0; j < size; j++)
        {
            idx = col_indices[ctr];
            val = vals[ctr++];
            if ((int)idx >= A.first_col && idx <= last_col)
            {
                A.on_proc.col_idx.push_back(idx - A.first_col);
                A.on_proc.data.push_back(val);
            }
            else
            {
                A.off_proc.col_idx.push_back(idx);
                A.off_proc.data.push_back(val);
            }
        }
        A.on_proc.rowptr[i+1] = A.on_proc.col_idx.size();
        A.off_proc.rowptr[i+1] = A.off_proc.col_idx.size();
    }
    A.on_proc.nnz = A.on_proc.col_idx.size();
    A.off_proc.nnz = A.off_proc.col_idx.size();

    std::map<int, int> orig_to_new;
    std::copy(A.off_proc.col_idx.begin(), A.off_proc.col_idx.end(),
            std::back_inserter(A.off_proc_columns));
    std::sort(A.off_proc_columns.begin(), A.off_proc_columns.end());

    int prev_col = -1;
    A.off_proc_num_cols = 0;
    for (std::vector<int>::iterator it = A.off_proc_columns.begin(); 
            it != A.off_proc_columns.end(); ++it)
    {
        //if (rank == 0) printf("*it, prev_col %d, %d\n", *it, prev_col);
        if (*it != prev_col)
        {
            orig_to_new[*it] = A.off_proc_num_cols;
            A.off_proc_columns[A.off_proc_num_cols++] = *it;
            prev_col = *it;
        }
    }
    A.off_proc_columns.resize(A.off_proc_num_cols);

    for (std::vector<int>::iterator it = A.off_proc.col_idx.begin();
            it != A.off_proc.col_idx.end(); ++it)
    {
        *it = orig_to_new[*it];
    }
}
