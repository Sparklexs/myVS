/*
 * VSEncodingOP.hpp
 *
 *  Created on: 2015年8月27日
 *      Author: John
 */

#ifndef INCLUDE_COMPRESS_POLICY_VSENCODINGOP_HPP_
#define INCLUDE_COMPRESS_POLICY_VSENCODINGOP_HPP_

#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <string>

namespace integer_encoding {
namespace internals {

typedef uint64_t posIndex_t;
typedef uint64_t cost_t;
typedef uint32_t element_t;

struct optimal_partition {

	/* partition内存的是序列的索引值，有可能超过int32，而序列元素最大才是64 */

	std::vector<posIndex_t> partition; // 每个划分的不包含上界[,)
	std::vector<element_t> Ks; // 每个划分包含的元素个数,长度比partition少1
	std::vector<element_t> Bs; // 每个划分中的最大位宽,长度比partition少1
	cost_t cost_opt = 0; // 暂时没什么用
	double eps1 = 0.03;
	double eps2 = 0.3;

	struct cost_window {
		// a window reppresent the cost of the interval [start, end)

		std::vector<element_t>::const_iterator start_it;
		std::vector<element_t>::const_iterator end_it;
		// starting and ending position of the window
		posIndex_t start = 0;
		posIndex_t end = 0; // end-th position is not in the current window
		element_t start_p = 0; // element that preceed the first element of the window
		element_t end_p = 0;
		cost_t cost_upper_bound;

		cost_window(std::vector<element_t>::const_iterator begin,
				cost_t cost_upper_bound) :
				start_it(begin), end_it(begin), start_p(*begin), end_p(0), cost_upper_bound(
						cost_upper_bound) {
		}
		element_t universe() const {
			return end_p - start_p + 1;
		}
		posIndex_t size() const {
			return end - start;
		}
		// 关于max和min暂时无用，因为传入进来的已经是经过remap之后的bitwidth(in[i])
		// 和in[i]没有直接关系了，
		void advance_start() {
			start_p = *start_it + 1;
			++start;
			++start_it;
		}
		void advance_end() {
			end_p = *end_it;
			++end;
			++end_it;
		}
	};
	optimal_partition() {
	}
	/*
	 * optimally partition an input sequence with random ks
	 */
	optimal_partition(const std::vector<element_t>& seq,
			cost_t fixedCost = 64) {

		FILE* stat = fopen("./share/oploops", "a");

		ASSERT(seq.size() != 0);

		//所有元素均使用32bit表示
		std::vector<element_t>::const_iterator begin = seq.begin();
		cost_t single_blcok_cost = seq.size() * 32;
		std::vector<cost_t> min_cost(seq.size() + 1, single_blcok_cost);
		min_cost[0] = 0;

		// create the required window: one for each power of approx_factor
		std::vector<cost_window> windows;
		cost_t cost_lower_bound = fixedCost;
		cost_t cost_bound = cost_lower_bound;
		while (/*eps1 == 0 ||*/cost_bound < cost_lower_bound / eps1) {
			//FIXME 由于begin作为每个窗口的滑动指针，如果是引用传递，那么操作一个begin++
			// 会不会导致所有的begin都向前移动？
			windows.emplace_back(begin, cost_bound);
			if (cost_bound >= single_blcok_cost)
				break;
			cost_bound = cost_bound * (1 + eps2);
		}
		// step 2: calculate optimal path
		std::vector<posIndex_t> path(seq.size() + 1, 0);
		std::vector<element_t> bPath(seq.size() + 1, 0);
		//XXX 1 in 3
		int ind = 0;

		bool firsttime;
		uint32_t firsttimeB = 0;
		for (posIndex_t i = 0; i < seq.size(); i++) {

			posIndex_t last_end = i + 1;
			element_t maxB = firsttimeB;
			firsttime = true;

			for (auto& window : windows) {
				assert(window.start == i);
				while (window.end < last_end) {
					window.advance_end();
				}
				cost_t window_cost;

				//从当前window的begin到endIndex之间计算maximum bitwidth
				if (UNLIKELY(maxB == 0)) {
					for (posIndex_t i = 0; i < window.size(); i++)
						maxB = maxB > *(window.start_it + i) ?
								maxB : *(window.start_it + i);
				}

				while (true) {
					//XXX 2 in 3
					ind++;
					maxB = maxB > window.end_p ? maxB : window.end_p;
					window_cost = window.size() * maxB;
					if (min_cost[i] + window_cost < min_cost[window.end]) {
						min_cost[window.end] = min_cost[i] + window_cost;
						path[window.end] = i;
						bPath[window.end] = maxB;
					}
					last_end = window.end;
					if (window.end == seq.size())
						break;
					if (window_cost >= window.cost_upper_bound)
						break;
					window.advance_end();
				}
				if (firsttime) {
					firsttime = false;
					if (maxB == window.start_p)
						firsttimeB = 0;
					else
						firsttimeB = maxB;
				}
				window.advance_start();
			}
		}
		/* for (int i = 0; i < bPath.size(); i++)
		 std::cout <<i<<":"<< bPath[i] << std::endl;*/
		//XXX 3 in 3
		//		std::cout << "times of loop: " << ind << std::endl;
		fwrite(&ind, 4, 1, stat);
		fflush(stat);
		fclose(stat);

		posIndex_t curr_pos = seq.size();
		posIndex_t last_pos = curr_pos;

		while (curr_pos != 0) {
			partition.emplace_back(curr_pos);
			Bs.emplace_back(bPath[curr_pos]);

			last_pos = curr_pos;
			curr_pos = path[curr_pos];
			Ks.emplace_back(last_pos - curr_pos);

		}
		partition.emplace_back(0);
		std::reverse(partition.begin(), partition.end());
		std::reverse(Bs.begin(), Bs.end());
		std::reverse(Ks.begin(), Ks.end());
		cost_opt = min_cost[seq.size()];
	}
	/*
	 * Optimally partition an input sequence with limited options for k
	 * seq represents the input integer sequence
	 * lens represents the array containning the options
	 */
	optimal_partition(const std::vector<element_t>& seq, const element_t *lens,
			const uint32_t size, cost_t fixedCost = 64) {
		ASSERT(seq.size() != 0);

		//所有元素均使用32bit表示
		std::vector<element_t>::const_iterator begin = seq.begin();
		cost_t single_blcok_cost = seq.size() * 32;
		std::vector<cost_t> min_cost(seq.size() + 1, single_blcok_cost);
		min_cost[0] = 0;

		// create the required window: one for each power of approx_factor
		std::vector<cost_window> windows;
		cost_t cost_lower_bound = fixedCost;
		cost_t cost_bound = cost_lower_bound;
		while (/*eps1 == 0 ||*/cost_bound < cost_lower_bound / eps1) {
			//FIXME 由于begin作为每个窗口的滑动指针，如果是引用传递，那么操作一个begin++
			// 会不会导致所有的begin都向前移动？
			windows.emplace_back(begin, cost_bound);
			if (cost_bound >= single_blcok_cost)
				break;
			cost_bound = cost_bound * (1 + eps2);
		}
		// step 2: calculate optimal path
		std::vector<posIndex_t> path(seq.size() + 1, 0);
		std::vector<element_t> bPath(seq.size() + 1, 0);

		for (posIndex_t i = 0; i < seq.size(); i++) {
			posIndex_t last_end = i + 1;

			// for each window search for minimum cost path
			for (auto& window : windows) {
				assert(window.start == i);
				while (window.end < last_end) {
					window.advance_end();
				}
				if (window.size() <= lens[size - 1]) {
					cost_t window_cost;

					//从当前window的begin到endIndex之间计算maximum bitwidth
					element_t maxB = 0;
					for (posIndex_t g = 0; g < window.size(); g++)
						maxB = maxB > *(window.start_it + g) ?
								maxB : *(window.start_it + g);
					int l = 0;
					while (l < size) {
						if (window.size() != lens[l])
							if (window.end != seq.size()) {
								last_end = window.end;
								window.advance_end();
								maxB = maxB > window.end_p ?
										maxB : window.end_p;
								continue;
							} else
								break;
						else
							l++;
						window_cost = window.size() * maxB;
						if (min_cost[i] + window_cost < min_cost[window.end]) {
							min_cost[window.end] = min_cost[i] + window_cost;
							path[window.end] = i;
							bPath[window.end] = maxB;
						}
						if (window_cost >= window.cost_upper_bound)
							break;
					}
				}
				window.advance_start();
			}
		}
		//		for (int i = 0; i < bPath.size(); i++)
		//			std::cout <<i<<":"<< bPath[i] << std::endl;

		posIndex_t curr_pos = seq.size();
		posIndex_t last_pos = curr_pos;

		while (curr_pos != 0) {
			partition.emplace_back(curr_pos);
			Bs.emplace_back(bPath[curr_pos]);

			last_pos = curr_pos;
			curr_pos = path[curr_pos];
			Ks.emplace_back(last_pos - curr_pos);

		}
		partition.emplace_back(0);
		std::reverse(partition.begin(), partition.end());
		std::reverse(Bs.begin(), Bs.end());
		std::reverse(Ks.begin(), Ks.end());
		cost_opt = min_cost[seq.size()];
	}
};
}		//namespace:internals
}		//namespace:integer_encoding

#endif /* INCLUDE_COMPRESS_POLICY_VSENCODINGOP_HPP_ */
