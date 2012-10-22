package a3t.explorer;

import java.util.Set;
import java.util.HashSet;
import java.util.Map;
import java.util.HashMap;

public class IndepGraph
{
	private final Map<Integer,Set<Integer>> edges = new HashMap();

	public void newNode(int n)
	{
		assert n >= 0;
		assert !edges.containsKey(n);
		edges.put(n, new HashSet());
	}
	
	public void addEdge(int m, int n)
	{
		edges.get(m).add(n);
		edges.get(n).add(m);
	}

	public Set<Integer> findGreedyMinVertexCover() {
		Set<Integer> cover = new HashSet();

		//add 0-degree nodes to cover
		for(Map.Entry<Integer,Set<Integer>> entry : edges.entrySet()) {
			Integer node = entry.getKey();
			if(entry.getValue().size() == 0) 
				cover.add(node);
		}
		for(Integer node : cover)
			edges.remove(node);
	
		int maxDegreeNode;
		while(true) {
			maxDegreeNode = findMaxDegreeNode();
			if(maxDegreeNode < 0)
				break;
			cover.add(maxDegreeNode);
			removeNode(maxDegreeNode);
		}
		return cover;
	}

	private int findMaxDegreeNode() {
		int maxDegree = 0;
		int maxNode = -1;
		for(Map.Entry<Integer,Set<Integer>> entry : edges.entrySet()) {
			Integer node = entry.getKey();
			Set<Integer> nbrs = entry.getValue();
			int size = nbrs.size();
			if(size > maxDegree) {
				maxDegree = size;
				maxNode = node;
			}
		}
		return maxNode;
	}

	private void removeNode(int node) {
		Set<Integer> nbrs = edges.remove(node);
		for(Integer nbr : nbrs) { 
			Set<Integer> nbrs2 = edges.get(nbr);
			nbrs2.remove(node);
		}
	}
}