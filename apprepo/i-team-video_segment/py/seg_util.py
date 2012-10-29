#!/usr/bin/python2.7

import segmentation_pb2 as seg
from segmentation_pb2 import SegmentationDesc as Segment

import bisect
import numpy as np
from numpy import linalg as la
import matplotlib.pyplot as plt
import matplotlib.patches as pat
import pdb

def CompareRegionById(lhs, rhs):
    return lhs.id < rhs.id

# Extend Region2D and CompoundRegion
Segment.Region2D.__lt__ = CompareRegionById
Segment.CompoundRegion.__lt__ = CompareRegionById

def CompareInterval(lhs, rhs):
    """Lexicographic sorting of intervals."""
    return (lhs.y < rhs.y) or (lhs.y == rhs.y and lhs.left_x < rhs.left_x)

# Extend ScanInterval
Segment.Rasterization.ScanInterval.__lt__ = CompareInterval

def LookupScanInterval(y, raster):
    """Locates first scanline with given y via binary search.
    If not found None is returned
    """
    scan_to_find = Segment.Rasterization.ScanInterval()
    scan_to_find.y = y
    scan_to_find.left_x = -1
    pos = bisect.bisect_left(raster.scan_inter, scan_to_find)

    if pos >= len(raster.scan_inter) or raster.scan_inter[pos].y != y:
        return None
    else:
        return pos

def LookupRegion2D(region_id, segmentation):
    """Locates region via binary search"""
    region_to_find = Segment.Region2D()
    region_to_find.id = region_id
    region_idx = bisect.bisect_left(segmentation.region, region_to_find)
    result = segmentation.region[region_idx]
    if result.id != region_id:
        raise LookupError("Region {} not found".format(region_id))
    return result

def LookupCompoundRegion(region_id, hier_level):
    """Locates region via binary search"""
    region_to_find = Segment.CompoundRegion()
    region_to_find.id = int(region_id)
    region_idx = bisect.bisect_left(hier_level.region, region_to_find)
    result = hier_level.region[region_idx]
    if result.id != region_id:
        raise LookupError("Region {} not found".format(region_id))
    return result


def GetParentId(region_id, level, query_level, hierarchy):
    """Traverse tree up the hierarchy and returns parent id at query_level"""
    if level > query_level:
        raise LookupError("Level {} is larger than query_level {}".format(
        level, query_level))

    if level == query_level:
        return region_id

    curr_region = LookupCompoundRegion(region_id, hierarchy[level]);
    return GetParentId(curr_region.parent_id, level + 1, query_level, hierarchy)

def GetParentMap(segmentation, hierarchy, level):
    """Maps parent ids at level to regions in segmentation"""
    parent_map = {}
    for region in segmentation.region:
        parent_id = GetParentId(region.id, 0, level, hierarchy)
        if parent_id not in parent_map:
            parent_map[parent_id] = [region]
        else:
            parent_map[parent_id].append(region)
    return parent_map

def GetParentsAtLevel(hierarchy, level):
    """Maps region ids to corresponding parent id at level
    """
    parent_map = {}
    for region in hierarchy[0].region:
        parent_map[region.id] = GetParentId(region.id, 0, level, hierarchy)

    return parent_map

def GetParentMatrix(hierarchy):
    """Maps each region to a list of parent ids for fast lookup.
    Stored as matrix, where A(i,j) represents the parent of region id i at level j
    """
    levels = len(hierarchy)
    num_regions =  len(hierarchy[0].region)
    parent_mat = np.zeros((num_regions, levels), dtype=np.int)
    parent_mat[:, 0] = range(0, num_regions)
    for l in range(1, levels):
        for r in range(0, num_regions) :
            parent_mat[r, l] = LookupCompoundRegion(parent_mat[r, l - 1],
                                                    hierarchy[l - 1]).parent_id
    return parent_mat

def RegionAtLocation(x, y, segmentation):
    x = int(x + 0.5)
    y = int(y + 0.5)

    for region in segmentation.region:
        scan_pos = LookupScanInterval(y, region.raster)
        if scan_pos is None:
            continue

        while scan_pos != len(region.raster.scan_inter) and \
              region.raster.scan_inter[scan_pos].y == y:
            scan_inter = region.raster.scan_inter[scan_pos]
            if scan_inter.left_x <= x <=scan_inter.right_x:
                return region
            scan_pos += 1
    raise LookupError("No region at position ({}, {}) found".format(x, y))

def NeighborCliques(region, hierarchy_level, depth, parent_map = None):
    """Returns neighbors of region at hierarchy level level
    by performing BFS over regions. Returned is a list of regions
    for each depth d from 0 to depth. If parent_map is specified
    only regions present in parent_map are used.
    """
    result = []
    # Add level 0.
    result.append([region])

    if parent_map is not None:
        assert region.id in parent_map

    visited = {region.id}
    for d in range(0, depth):
        depth_neighbors = []
        for r in result[d]:
           # Add all unvisited neighbors to depth_neighbors
           for n in r.neighbor_id:
               if n in visited:
                   continue

               # Skip regions not in parent map
               if parent_map is not None and n not in parent_map:
                  continue

               visited.add(n)
               depth_neighbors.append(LookupCompoundRegion(n, hierarchy_level))
        result.append(depth_neighbors)
    return result

def RegionIDImage(segmentation, hierarchy, level):
    width = segmentation.frame_width
    height = segmentation.frame_height
    id_image = np.zeros( (height, width), dtype=np.int)
    parent_list = GetParentsAtLevel(hierarchy, level);

    for region in segmentation.region:
        id_to_print = parent_list[region.id];
        for interval in region.raster.scan_inter:
            id_image[interval.y, interval.left_x:interval.right_x+1] = id_to_print
    return id_image;

def RenderSegmentation(segmentation, hierarchy, level,
                       output=False, parent_matrix=None,
                       viz_shape=False):
    width = segmentation.frame_width
    height = segmentation.frame_height
    id_image = np.zeros((height, width))
    if parent_matrix is None:
        parent_list = GetParentsAtLevel(hierarchy, level)

    for region in segmentation.region:
        if parent_matrix is None:
            np.random.seed(parent_list[region.id])
        else:
            np.random.seed(parent_matrix[region.id, level])
        id_to_print = np.random.random()
        for interval in region.raster.scan_inter:
            id_image[interval.y, interval.left_x:interval.right_x+1] = id_to_print

    if output:
        fig = plt.figure()
        ax = fig.add_subplot(111)
        ax.imshow(id_image)
        if viz_shape:
            parent_map = GetParentMap(segmentation, hierarchy, level)
            for (parent, children) in parent_map.iteritems():
                sd = ShapeDescriptorFromRegions(children)
                if sd is None:
                    continue
                e = pat.Ellipse(sd.center, 2*la.norm(sd.major), 2*la.norm(sd.minor),
                                np.arctan2(sd.major[1], sd.major[0]) / np.pi * 180.0,
                                fill=False)
                ax.add_artist(e)
        plt.show()

    return id_image;

def RenderSegVideo(segs, hierarchy, level) :
    def press(event):
       print 'press', event.key
       if event.key == 'n':
           press.curr_frame += 1
       if event.key == 'b':
           press.curr_frame -= 1
       if event.key == 'j':
           press.curr_frame += 20
       if event.key == 'h':
           press.curr_frame -= 20
       press.curr_frame = max(0, min(press.curr_frame, len(segs) - 1))
       ax.imshow(RenderSegmentation(segs[press.curr_frame], hierarchy, level,
                                    parent_matrix=parent_matrix),
                 interpolation='nearest')
       fig.canvas.draw()


    press.curr_frame = 0;
    parent_matrix = GetParentMatrix(hierarchy)
    fig = plt.figure()
    ax = fig.add_subplot(111)
    fig.canvas.mpl_connect('key_press_event', press)
    render_seg_curr_view = 0;
    ax.imshow(RenderSegmentation(segs[0], hierarchy, level, parent_matrix=parent_matrix),
              interpolation='nearest')

    plt.show()

class ShapeDescriptor:
    def set_center(self, x, y):
        self.center = np.array([x, y])

    def set_major_vec(self, v):
        self.major = v
        self.major_n2 = self.major.dot(self.major)

    def set_minor_vec(self, v):
        self.minor = v
        self.minor_n2 = self.minor.dot(self.minor)

    def set_major(self, v_x, v_y):
        self.major = np.array([v_x, v_y])
        self.major_n2 = self.major.dot(self.major)

    def set_minor(self, v_x, v_y):
        self.minor = np.array([v_x, v_y])
        self.minor_n2 = self.minor.dot(self.minor)

    def CoordsForPoint(self, pt):
        """Returns coefficient of pt w.r.t. major and minor axis"""
        ptc = pt - self.center
        # Project on each axis
        major_norm = self.major / self.major_n2
        assert self.minor_n2 > 0, "Boom {}".format(self.minor.dot(self.minor))

        minor_norm = self.minor / self.minor_n2
        return np.array([major_norm.dot(ptc), minor_norm.dot(ptc)])

    def PointFromCoords(self, coords):
        """Returns point location given coordinates"""
        return self.center + coords[0] * self.major + coords[1] * self.minor

    def Eccentricity(self):
        assert self.major_n2 != 0
        ecc = 1 - self.minor_n2 / self.major_n2
        assert(ecc >= 0), "Major {}, Minor {}".format(self.major_n2, self.minor_n2)
        return np.sqrt(ecc)

def ShapeDescriptorFromRegion(region):
    return ShapeDescriptorFromRegions([region])

def ShapeDescriptorFromRegions(regions):
    # Sum moments of regions.
    mixed_x = 0.0
    mixed_y = 0.0
    mixed_xx = 0.0
    mixed_xy = 0.0
    mixed_yy = 0.0
    area_sum = 0.0
    for r in regions:
        area = r.shape_moments.size
        area_sum += area
        mixed_x += r.shape_moments.mean_x * area
        mixed_y += r.shape_moments.mean_y * area
        mixed_xx += r.shape_moments.moment_xx * area
        mixed_xy += r.shape_moments.moment_xy * area
        mixed_yy += r.shape_moments.moment_yy * area

    # Skip if too small.
    if area_sum < 10:
        return None

    # Normalize.
    mixed_x /= area_sum
    mixed_y /= area_sum
    mixed_xx /= area_sum
    mixed_xy /= area_sum
    mixed_yy /= area_sum

    sd = ShapeDescriptor()
    sd.set_center(mixed_x, mixed_y)

    # Major and minor axis are eigenvectors of variance matrix
    var_xx = mixed_xx - mixed_x * mixed_x
    var_xy = mixed_xy - mixed_x * mixed_y
    var_yy = mixed_yy - mixed_y * mixed_y

    var = np.array([ [var_xx, var_xy], [var_xy, var_yy] ])
    (eig_vals, eig_vectors) = la.eig(var)

    if eig_vals[0] < 0:
        eig_vals[0] *= -1
        eig_vectors[:, 0] *= -1
    if eig_vals[1] < 0:
        eig_vals[1] *= -1
        eig_vectors[:, 1] *= -1

    if eig_vals[0] < eig_vals[1]:
      eig_vals = eig_vals[::-1]
      eig_vectors = eig_vectors[:, ::-1]

    # Skip region is just one pixel row / column.
    if (eig_vals[1] < 1e-4):
        return None

    sd.set_major_vec(eig_vectors[:, 0] * np.sqrt(eig_vals[0]))
    sd.set_minor_vec(eig_vectors[:, 1] * np.sqrt(eig_vals[1]))
    return sd

def ShapeDescriptorsAtLevel(segmentation, hierarchy, level, parent_map = None):
    """ Returns map of compound region id to shape descriptor"""
    descriptors = {}
    if parent_map is None:
        parent_map = GetParentMap(segmentation, hierarchy, level)

    for (parent_id, children) in parent_map.iteritems():
        sd = ShapeDescriptorFromRegions(children)
        if sd is None:
            continue
        descriptors[parent_id] = sd
    return descriptors

def LocateSupportRegions(point, segmentation, hierarchy, level, neighbor_rad,
                         parent_map = None):
    """ Returns neighbor clique of compound regions around query point."""
    region = RegionAtLocation(point[0], point[1], segmentation)
    comp_region = LookupCompoundRegion(GetParentId(region.id, 0, level, hierarchy),
                                       hierarchy[level])

    return NeighborCliques(comp_region, hierarchy[level], neighbor_rad, parent_map)

def ShapeFeatures(curr_shape, prev_shape, pt):
    """Returns vector of features derived from current shape descriptor
       and previous shape descriptor.
    """
    features = []
    # Compute spatial features (within a frame)
    # Eccentricity.
    features.append(curr_shape.Eccentricity())
    features.append(prev_shape.Eccentricity())

    # Shape in general.
    features.append(np.sqrt(curr_shape.major_n2))
    features.append(np.sqrt(curr_shape.minor_n2))
    features.append(features[-2] * features[-1])  # Area like measure

    features.append(np.sqrt(prev_shape.major_n2))
    features.append(np.sqrt(prev_shape.minor_n2))
    features.append(features[-2] * features[-1])

    # Distance to center.
    curr_diff = pt - curr_shape.center
    prev_diff = pt - prev_shape.center
    features.append(np.sqrt(curr_diff.dot(curr_diff)))
    features.append(np.sqrt(prev_diff.dot(prev_diff)))

    # Compute temporal features (across frames).
    center_diff = curr_shape.center - prev_shape.center
    major_diff = abs(curr_shape.major_n2 - prev_shape.major_n2)
    minor_diff = abs(curr_shape.minor_n2 - prev_shape.minor_n2)
    features.append(np.sqrt(center_diff.dot(center_diff)))
    features.append(major_diff)
    features.append(minor_diff)

    angle_diff = \
        (curr_shape.major / np.sqrt(curr_shape.major_n2)).dot(  \
        (prev_shape.major / np.sqrt(prev_shape.major_n2)))
    features.append(angle_diff)
    features.append(1.0)

    return features


def ConcatenateCompoundRegions(region_1, region_2):
    """Merges two compound regions of the same id at the same hierarchy level"""
    assert region_1.id == region_2.id, "Regions have different ids."
    assert region_1.parent_id == region_2.parent_id, "Incompatible parent ids."

    merged = Segment.CompoundRegion()
    merged.id = region_1.id
    merged.size = region_1.size + region_2.size
    merged.parent_id = region_1.parent_id

    # Merge children and neighbors (result unique and sorted).
    merged.neighbor_id.extend(list(set(list(region_1.neighbor_id) +
                                       list(region_2.neighbor_id))))
    merged.neighbor_id.sort()

    merged.child_id.extend(list(set(list(region_1.child_id) +
                                    list(region_2.child_id))))
    merged.child_id.sort()

    merged.start_frame = min(region_1.start_frame, region_2.start_frame)
    merged.end_frame = max(region_1.end_frame, region_2.end_frame)
    return merged

def MergeHierarchyLevel(level_1, level_2):
  def IncrementOrFinish(iterator, other_iter, merged):
    try:
      return iterator.next()
    except StopIteration:
      merged.extend([remaining for remaining in other_iter])
      return None

  # Order preserving merge of regions.
  merged = Segment.HierarchyLevel()

  region_1_iter = iter(level_1.region)
  region_2_iter = iter(level_2.region)

  region_1 = IncrementOrFinish(region_1_iter, region_2_iter, merged.region)
  region_2 = IncrementOrFinish(region_2_iter, region_1_iter, merged.region)

  while region_1 and region_2:
    if region_1.id < region_2.id:
      merged.region.extend([region_1])
      region_1 = IncrementOrFinish(region_1_iter, region_2_iter, merged.region)
    elif region_2.id < region_1.id:
      merged.region.extend([region_2])
      region_2 = IncrementOrFinish(region_2_iter, region_1_iter, merged.region)
    else:
      assert region_1.id == region_2.id
      merged.region.extend([ConcatenateCompoundRegions(region_1, region_2)])
      region_1 = IncrementOrFinish(region_1_iter, region_2_iter, merged.region)
      region_2 = IncrementOrFinish(region_2_iter, region_1_iter, merged.region)

  return merged

def TruncateHierarchy(to_levels, hierarchy):
  """Remove all levels above to_levels and sets parents to -1 for last remaining level."""
  if len(hierarchy) < to_levels:
    return

  while len(hierarchy) > to_levels:
    hierarchy.pop()

  for region in hierarchy[-1].region:
    region.parent_id = -1

def BuildGlobalHierarchy(chunk_hierarchy, chunk_start, global_hierarchy):
  """Builds global hierarchy iteratively from chunk_hierarchies that start at frame
  chunk_start """
  if len(global_hierarchy) == 0:
    global_hierarchy.extend(chunk_hierarchy)
    return

  if len(global_hierarchy) > len(chunk_hierarchy):
    TruncateHierarchy(len(chunk_hierarchy), global_hierarchy)

  merged = []
  for (level_idx, chunk_level) in enumerate(chunk_hierarchy):
    global_level = global_hierarchy[level_idx]
    clear_parents = False
    if level_idx + 1 == len(chunk_hierarchy) and \
       len(global_hierarchy) < len(chunk_hierarchy):
      clear_parents = True

    level_copy = Segment.HierarchyLevel()
    level_copy.CopyFrom(chunk_level)

    for region in level_copy.region:
      if clear_parents:
        region.parent_id = -1
      region.start_frame += chunk_start
      region.end_frame += chunk_start

    merged.append(MergeHierarchyLevel(level_copy, global_level))

  # Reset list.
  global_hierarchy[:] = []
  global_hierarchy.extend(merged)
