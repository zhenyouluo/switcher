/*
 * This file is part of switcher-jack.
 *
 * switcher-myplugin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <iostream>

namespace switcher {

template<typename TimeType>
TimeType DriftObserver<TimeType>::set_current_time_info(
    const TimeType date,
    const TimeType duration){
  // udating statistics for the previous duration
  if (0 != current_buffer_duration_) {
    double measured_ratio = (double)(date - current_buffer_date_) / current_buffer_duration_;
    if (0.9 < measured_ratio && measured_ratio < 1.1)
      ratio_ = (1 - smoothing_factor_) * ratio_ + smoothing_factor_ * measured_ratio;
    else
      ratio_ = 1;
  }
  current_buffer_date_ = date;
  current_buffer_duration_ = duration;
  // computing expected duration for this current data,
  // according to statistics previouslyt computed
  double res = duration*ratio_ + remainder_;
  //std::cout << "new duration " << res << std::endl;
  remainder_ = res - static_cast<TimeType>(res);
  return static_cast<TimeType>(res);
}

}  // namespace switcher
