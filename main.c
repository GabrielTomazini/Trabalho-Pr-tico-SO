#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define PAGE_SIZE 4096            // 4KB
#define FRAME_COUNT 64            // 64 frames na memória física
#define TLB_SIZE 16               // Tamanho da TLB
#define PAGE_TABLE_SIZE (1 << 20) // 2^20 páginas

typedef struct
{
    uint32_t page_number;
    uint32_t frame_number;
    bool valid;
    bool reference;
    int lru_counter;
} PageTableEntry;

typedef struct
{
    uint32_t page_number;
    uint32_t frame_number;
    bool valid;
} TLBEntry;

PageTableEntry page_table[PAGE_TABLE_SIZE];
TLBEntry tlb[TLB_SIZE];
int frame_usage[FRAME_COUNT] = {0};
int lru_counter = 0;
long int tlb_miss = 0, tlb_hits = 0, page_faults = 0;

void init_page_table()
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        page_table[i].valid = false;
        page_table[i].reference = false;
        page_table[i].lru_counter = 0;
    }
}

void init_tlb()
{
    for (int i = 0; i < TLB_SIZE; i++)
    {
        tlb[i].valid = false;
    }
}

int search_tlb(uint32_t page_number)
{
    for (int i = 0; i < TLB_SIZE; i++)
    {
        if (tlb[i].valid && tlb[i].page_number == page_number)
        {
            return tlb[i].frame_number;
        }
    }
    return -1;
}

void update_tlb(uint32_t page_number, uint32_t frame_number)
{
    static int tlb_index = 0;
    tlb[tlb_index].page_number = page_number;
    tlb[tlb_index].frame_number = frame_number;
    tlb[tlb_index].valid = true;
    tlb_index = (tlb_index + 1) % TLB_SIZE;
}

int find_free_frame()
{
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        if (frame_usage[i] == 0)
            return i;
    }
    return -1;
}

int lru_replacement()
{
    int oldest = -1;
    int min_counter = INT_MAX;
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (page_table[i].valid && page_table[i].lru_counter < min_counter)
        {
            min_counter = page_table[i].lru_counter;
            oldest = i;
        }
    }
    return page_table[oldest].frame_number;
}

int second_chance_replacement()
{
    static int pointer = 0;
    while (true)
    {
        if (!page_table[pointer].reference)
        {
            return page_table[pointer].frame_number;
        }
        page_table[pointer].reference = false;
        pointer = (pointer + 1) % PAGE_TABLE_SIZE;
    }
}

void handle_page_fault(uint32_t page_number, int policy)
{
    int frame_number = find_free_frame();
    if (frame_number == -1)
    {
        int victim_page = (policy == 0) ? lru_replacement() : second_chance_replacement();
        frame_number = page_table[victim_page].frame_number;
        page_table[victim_page].valid = false;
    }
    page_table[page_number].frame_number = frame_number;
    page_table[page_number].valid = true;
    frame_usage[frame_number] = 1;
    update_tlb(page_number, frame_number);
    page_faults++;
}

void process_address(uint32_t address, int policy)
{
    uint32_t page_number = address >> 12;
    uint32_t offset = address & 0xFFF;
    int frame_number = search_tlb(page_number);

    if (frame_number == -1)
    {
        tlb_miss++;
        if (!page_table[page_number].valid)
        {
            handle_page_fault(page_number, policy);
        }
        frame_number = page_table[page_number].frame_number;
        update_tlb(page_number, frame_number);
    }
    else
    {
        tlb_hits++;
    }

    if (policy == 0)
    {
        page_table[page_number].lru_counter = lru_counter++;
    }
    else
    {
        page_table[page_number].reference = true;
    }

    uint32_t physical_address = (frame_number << 12) | offset;
    printf("Endereço Lógico: 0x%08X -> Endereço Físico: 0x%08X\n", address, physical_address);
}

void read_trace_file(const char *filename, int policy)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Erro na abertura do arquivo!");
        exit(1);
    }
    uint32_t address;
    char type;
    while (fscanf(file, "%x %c", &address, &type) != EOF)
    {
        process_address(address, policy);
    }
    fclose(file);
}

int main(int argc, char *argv[])
{

    int policy = atoi(argv[2]);

    if (argc != 3)
    {
        printf("Uso correto: %s <nome do arquivo de trace> <política (0 = LRU, 1 = Segunda chance)>\n", argv[0]);
        return 1;
    }
    if (policy != 0 && policy != 1)
    {
        printf("Política inválida! Use 0 para LRU ou 1 para Segunda Chance.\n");
        return 1;
    }

    init_page_table();
    init_tlb();
    read_trace_file(argv[1], policy);

    if (policy)
        printf("\nMetodo utilizado: Segunda Chance\n");
    else
        printf("\nMetodo utilizado: LRU\n");

    printf("Page Faults: %ld\n", page_faults);
    printf("TLB Hits: %ld\n", tlb_hits);
    printf("TLB Misses: %ld\n", tlb_miss);
    return 0;
}