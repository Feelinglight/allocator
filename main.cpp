#include <iostream>
#include <cassert>
#include <list>
#include <stack>
#include <vector>
#include <optional>
#include <iterator>
#include <algorithm>
using namespace std;

#define MEMORY_CAPACITY 1000

enum class mem_state_t
{
    busy,
    free,
    invalid
};

struct mem_area_t
{
    size_t start = 0;
    size_t length = 0;
    mem_state_t state;

    mem_area_t(size_t a_start, size_t a_length, mem_state_t a_state) :
    start(a_start),
    length(a_length),
    state(a_state)
    {}
};

using mem_areas_list_t = list<mem_area_t>;
using last_freed_mem_areas_t = stack<mem_areas_list_t::iterator>;
using cells_to_mem_area_map_t = vector<optional<mem_areas_list_t::iterator>>;

mem_areas_list_t& __get_mem_areas_list()
{
    static mem_areas_list_t mem_areas {
        mem_area_t {0, MEMORY_CAPACITY, mem_state_t::free}
    };
    return mem_areas;
}

mem_areas_list_t& __get_invalid_mem_areas_list()
{
    // Сюда перемещаются невалидные отрезки, которые были смержены при освобождении,
    // чтобы их память не удалялась, и в стеке не было невалидных итераторов
    static mem_areas_list_t invalid_mem_areas;
    return invalid_mem_areas;
}

last_freed_mem_areas_t& __get_last_freed_mem_areas()
{
    static last_freed_mem_areas_t last_freed_mem_areas {
        last_freed_mem_areas_t::container_type { __get_mem_areas_list().begin() },
    };
    return last_freed_mem_areas;
}

cells_to_mem_area_map_t& __get_cells_to_mem_area_map()
{
    static cells_to_mem_area_map_t cells_to_mem_area_map(MEMORY_CAPACITY);
    return cells_to_mem_area_map;
}

// Для использования в assert
size_t __free_space()
{
    size_t free = 0;
    for (auto& mem_area: __get_mem_areas_list()) {
        assert(mem_area.state != mem_state_t::invalid);
        if (mem_area.state == mem_state_t::free) {
            free += mem_area.length;
        }
    }
    return free;
}

int malloc_(size_t a_size)
{
    auto& last_freed_areas = __get_last_freed_mem_areas();
    assert(last_freed_areas.empty() ? __free_space() == 0 : __free_space() != 0);

    if (!last_freed_areas.empty() && last_freed_areas.top()->length >= a_size) {
        mem_areas_list_t::iterator last_freed_area = last_freed_areas.top();
        assert(last_freed_area->state != mem_state_t::invalid);
        last_freed_areas.pop();

        size_t new_addr = last_freed_area->start;

        auto& mem_areas = __get_mem_areas_list();
        mem_areas_list_t::iterator new_busy_area = mem_areas.insert(
        last_freed_area, mem_area_t { new_addr, a_size, mem_state_t::busy });

        if (last_freed_area->length > a_size) {
            // Добавляем оставшуюся свободную область
            mem_areas_list_t::iterator new_free_area = mem_areas.insert(last_freed_area,
            mem_area_t { new_busy_area->start + new_busy_area->length,
            last_freed_area->length - a_size, mem_state_t::free } );
            last_freed_areas.push(new_free_area);
        }
        mem_areas.erase(last_freed_area);

        auto& cells_to_mem_area_map = __get_cells_to_mem_area_map();
        assert(!cells_to_mem_area_map[new_addr].has_value());

        cells_to_mem_area_map[new_addr] = new_busy_area;

        return static_cast<int>(new_addr);
    } else {
        return -1;
    }
}

int free_(int a_address)
{
    assert(a_address >= 0);
    auto& cells_to_mem_area_map = __get_cells_to_mem_area_map();
    if (cells_to_mem_area_map[a_address].has_value()) {
        mem_areas_list_t::iterator freed_area = *cells_to_mem_area_map[a_address];
        cells_to_mem_area_map[a_address].reset();

        freed_area->state = mem_state_t::free;

        auto& mem_areas = __get_mem_areas_list();
        auto& invalid_mem_areas_list = __get_invalid_mem_areas_list();

        mem_areas_list_t::iterator next_to_free = next(freed_area);
        if (next_to_free != mem_areas.end() && next_to_free->state == mem_state_t::free) {
            // Мердж с правым свободным
            freed_area->length += next_to_free->length;

            next_to_free->state = mem_state_t::invalid;
            invalid_mem_areas_list.splice(invalid_mem_areas_list.end(), mem_areas, next_to_free);
        }
        if (freed_area != mem_areas.begin()) {
            // Мердж с левым свободным
            mem_areas_list_t::iterator prev_to_free = prev(freed_area);
            if (prev_to_free->state == mem_state_t::free) {
                freed_area->start = prev_to_free->start;
                freed_area->length += prev_to_free->length;

                prev_to_free->state = mem_state_t::invalid;
                invalid_mem_areas_list.splice(invalid_mem_areas_list.end(), mem_areas, prev_to_free);
            }
        }

        auto& last_freed_areas = __get_last_freed_mem_areas();
        // Снимаем со стека невалидные отрезки, до того как положить последний освобожденный
        while (!last_freed_areas.empty() && last_freed_areas.top()->state == mem_state_t::invalid) {
            mem_areas_list_t::iterator last_freed_area = last_freed_areas.top();

            last_freed_areas.pop();
            invalid_mem_areas_list.erase(last_freed_area);
        }

        last_freed_areas.push(freed_area);

        return 0;
    } else {
        return -1;
    }
}
