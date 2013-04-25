

class fib39{
    static int fib(int n){
        if(n < 2) return n;
        return fib(n-2) + fib(n-1);
    }
    
    public static void main(String[] argc){
        System.out.println(fib(39));
    }
}