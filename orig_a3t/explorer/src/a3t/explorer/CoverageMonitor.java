package a3t.explorer;

import java.util.Set;
import java.util.HashSet;
import java.util.List;
import java.util.Iterator;
import java.io.File;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;

public class CoverageMonitor
{
    // Set of <sid>'s of conds in app code whose T branch was taken in some run at least once.
    private static Set<Integer> trueBranches = new HashSet();
    // Set of <sid>'s of conds in app code whose F branch was taken in some run at least once.
    private static Set<Integer> falseBranches = new HashSet();
    // Set of <sid>'s of conds in app code whose exactly one branch (T or F) was taken in some
    // run at least once and whose other branch was never taken in any run.
    private static Set<Integer> dangBranches = new HashSet();

    private int gain;
    private int count;
    private int prevDanglingBranches;
    
    CoverageMonitor(int runId)
    {
        prevDanglingBranches = dangBranches.size();
    }

    /*
        Called once for each A3T_APP, i.e., each time assume is called on a cond in app code.
        There are two cases:
        1. If meta-data of cond is null: entry is of form "[T|F]<sid>"
        2. If meta-data of cond is non-null: entry is of form "[T|F]<sid> <did>"
        This method ignores the <did> in case 2 above.
    */
    void process(String entry)
    {
        count++;
        int i = entry.indexOf(' ');
        if (i < 0) {    // no metadata
            i = entry.length(); 
        }
        String bidStr = entry.substring(0, i);
        char decision = bidStr.charAt(0);
        int bid = Integer.parseInt(bidStr.substring(1));
        switch (decision) {
        case 'T':
            if(addBranch(bid, trueBranches, falseBranches))
                gain++;
            break;
        case 'F':
            if(addBranch(bid, falseBranches, trueBranches))
                gain++;
            break;
        default:
            throw new RuntimeException("unexpected " + entry);
        }
    } 

    void finish()
    {
        int n = dangBranches.size() - prevDanglingBranches;
        System.out.println("branch cov. = " + count + " (" + gain + " new)");
        System.out.println("change in dang. branches = " + (n > 0 ? "+" : "") + n + " (" + dangBranches.size() + " total)");
    }

    private boolean addBranch(int bid, Set a, Set b)
    {
        boolean ret = a.add(bid);
        if(ret){
            if(b.contains(bid)){
                //the other branch of this stmt was covered earlier
                dangBranches.remove(bid);
            }
            else{
                dangBranches.add(bid);
            }
        }
        return ret;
    }
    
    // Goes over each cond in given file (presumably file of all static conds in app code)
    // and if the <sid> of that cond is in dangBranches (i.e., one of the T|F branches
    // of that cond was taken in at least some run and the other was never taken in any
    // run), then prints that <sid> along with a list of lists:
    static void printDangBranches(String cond_mapTxtFileName)
    {
        if(cond_mapTxtFileName == null)
            return;
        File file = new File(cond_mapTxtFileName);
        if(!file.exists())
            return;
        System.out.println("=========== Dangling branches ===========");
        try{
            BufferedReader reader = new BufferedReader(new FileReader(file));
            String line = reader.readLine();
            if (line == null) {
                System.out.println("empty cond map " + cond_mapTxtFileName);
                return;
            }
            int id = Integer.parseInt(line.substring(0,line.indexOf(":")));
            while(line != null){
                if(dangBranches.contains(id)){
                    String target;
                    if(trueBranches.contains(id)){
                        target = "F"+id;
                        System.out.println("F " + line);
                    }
                    else if(falseBranches.contains(id)){
                        target = "T"+id;
                        System.out.println("T " + line);
                    }
                    else
                        throw new Error("unexpected " + id);
                }
                id++;
                line = reader.readLine();
            }
            reader.close();
        }catch(IOException e){
            throw new Error(e);
        }
    }

}
