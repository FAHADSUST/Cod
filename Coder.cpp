 /**
  * Copyright (C) 2013 - Francesc Auli-Llinas
  *
  * This program is distributed under the BOI License.
  * This program is distributed in the hope that it will be useful, but without any
  * warranty; without even the implied warranty of merchantability or fitness for a particular purpose.
  * You should have received a copy of the BOI License along with this program. If not,
  * see <http://www.deic.uab.cat/~francesc/software/license/>.
  */
 package coders;
 
 import streams.ByteStream;
 
 
 /**
  * This class implements an arithmetic coder. The underlying coding scheme is based on the
  * arithmetic coder MQ defined in the JPEG2000 standard (see the <code>ArithmeticCoderMQ</code> class
  * for more details). This class can perform both the operations of the encoder and the decoder.<br>
  *
  * Usage: once the object is created, the functions to code/decode symbols are used to code the
  * message. Instead of destroying the object and creating another one to encode a new message, it
  * is more computationally efficient to reuse the same object. When encoding, the same object can be
  * reused by calling the functions <code>terminate</code>, get the stream wherever is needed,
  * <code>changeStream</code>, <code>restartEncoding</code> and <code>reset</code> in this order.
  * To reuse the decoder, the functions <code>changeStream</code>, <code>restartDecoding</code>, and
  * <code>reset</code> have to be called in this order.<br>
  *
  * Multithreading support: the object must be created and manipulated by a single thread. There
  * can be many objects of this class running simultaneously as long as a single thread manipulates
  * each object.<br>
  *
  * @author Francesc Auli-Llinas
  * @version 1.0
  */
 public final class ArithmeticCoder{
 
   /**
    * ByteStream employed by the coder to write/read the output/input bytes.
    * <p>
    * The stream may contain zero bytes.
    */
   private ByteStream stream;
 
   /**
    * Interval range.
    * <p>
    * From right to left: 8 register bits, 3 spacer bits, 8 partial code bits, and 1 carry bit.
    */
   private int A;
 
   /**
    * Lower down interval.
    * <p>
    * From right to left: 8 register bits, 3 spacer bits, 8 partial code bits, and 1 carry bit.
    */
   private int C;
 
   /**
    * Number of bits to transfer.
    * <p>
    * It is set to 8 except when carry situations occur, in which is set to 7.
    */
   private int t;
 
   /**
    * Byte to flush out/read.
    * <p>
    * Flushed byte to the stream.
    */
   private int Tr;
 
   /**
    * Current byte read/write.
    * <p>
    * Current position in the stream.
    */
   private int L;
 
   /**
    * Number of contexts.
    * <p>
    * Set when the class is instantiated.
    */
   private int numContexts = -1;
 
   /**
    * Current state of the context.
    * <p>
    * Must in the range [0, STATE_TRANSITIONS_MPS.length - 1]
    */
   private int[] contextState = null;
 
   /**
    * Most probable symbol of the Context.
    * <p>
    * Either 0 or 1.
    */
   private int[] contextMPS = null;
 
   /**
    * Transition to the next state when coding the most probable symbol.
    * <p>
    * Each index must in the range [0, STATE_TRANSITIONS_MPS.length - 1]
    */
   private static final int[] STATE_TRANSITIONS_MPS = {1, 2, 3, 4, 5, 38, 7, 8, 9, 10,
     11, 12, 13, 29, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
     31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 45, 46};
 
   /**
    * Transition to the next state when coding the least probable symbol.
    * <p>
    * Each index must in the range [0, STATE_TRANSITIONS_MPS.length - 1]
    */
   private static final int[] STATE_TRANSITIONS_LPS = {1, 6, 9, 12, 29, 33, 6, 14, 14,
     14, 17, 18, 20, 21, 14, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 23, 24, 25, 26,
     27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 46};
 
   /**
    * Most probable symbol change.
    * <p>
    * 1 indicates a change of the MPS.
    */
   private static final int[] STATE_CHANGE = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
     0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0};
 
   /**
    * Probability of the context.
    * <p>
    * The real probability can be computed as the coded probability expressed in this array 0xXXXX / (2^16 * \alpha), with \alpha = 0.708.
    */
   private static final int[] STATE_PROB = {0x5601, 0x3401, 0x1801, 0x0AC1, 0x0521,
     0x0221, 0x5601, 0x5401, 0x4801, 0x3801, 0x3001, 0x2401, 0x1C01, 0x1601, 0x5601,
     0x5401, 0x5101, 0x4801, 0x3801, 0x3401, 0x3001, 0x2801, 0x2401, 0x2201, 0x1C01,
     0x1801, 0x1601, 0x1401, 0x1201, 0x1101, 0x0AC1, 0x09C1, 0x08A1, 0x0521, 0x0441,
     0x02A1, 0x0221, 0x0141, 0x0111, 0x0085, 0x0049, 0x0025, 0x0015,  0x0009, 0x0005,
     0x0001, 0x5601};
 
   /**
    * Bit masks (employed when coding integers).
    * <p>
    * The position in the array indicates the bit for which the mask is computed.
    */
   protected static final int[] BIT_MASKS = {1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5,
     1 << 6, 1 << 7, 1 << 8, 1 << 9, 1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15,
     1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22, 1 << 23, 1 << 24,
     1 << 25, 1 << 26, 1 << 27, 1 << 28, 1 << 29, 1 << 30};
 
 
   /**
    * Initializes internal registers. Before using the coder, a stream has to be set
    * through <code>changeStream</code>.
    */
   public ArithmeticCoder(){
     reset();
     restartEncoding();
   }
 
   /**
    * Initializes internal registers and creates the number of contexts specified. Before
    * using the coder, a stream has to be set through <code>changeStream</code>.
    *
    * @param numContexts number of contexts available for this object
    */
   public ArithmeticCoder(int numContexts){
     this.numContexts = numContexts;
     contextState = new int[numContexts];
     contextMPS = new int[numContexts];
     reset();
     restartEncoding();
   }
 
   /**
    * Encodes a bit using a context so that the probabilities are adaptively adjusted
    * depending on the incoming symbols.
    *
    * @param bit input
    * @param context context of the symbol
    */
   public void encodeBitContext(boolean bit, int context){
     int x = bit ? 1 : 0;
     int s = contextMPS[context];
     int p = STATE_PROB[contextState[context]];
 
     A -= p;
     if(x == s){ //Codes the most probable symbol
       if(A >= (1 << 15)){
         C += p;
       }else{
         if(A < p){
           A = p;
         }else{
           C += p;
         }
         contextState[context] = STATE_TRANSITIONS_MPS[contextState[context]];
         while(A < (1 << 15)){
           A <<= 1;
           C <<= 1;
           t--;
           if(t == 0){
             transferByte();
           }
         }
       }
     }else{ //Codes the least probable symbol
       if(A < p){
         C += p;
       }else{
         A = p;
       }
       if(STATE_CHANGE[contextState[context]] == 1){
         contextMPS[context] = contextMPS[context] == 0 ? 1: 0; //Switches MPS/LPS if necessary
       }
       contextState[context] = STATE_TRANSITIONS_LPS[contextState[context]];
       while(A < (1 << 15)){
         A <<= 1;
         C <<= 1;
         t--;
         if(t == 0){
           transferByte();
         }
       }
     }
   }
 
   /**
    * Decodes a bit using a context so that the probabilities are adaptively adjusted
    * depending on the outcoming symbols.
    *
    * @param context context of the symbols
    * @return output bit
    * @throws Exception when some problem manipulating the stream occurs
    */
   public boolean decodeBitContext(int context) throws Exception{
     int p = STATE_PROB[contextState[context]];
     int s = contextMPS[context];
     int x = s;
 
     A -= p;
     if((C & 0x00FFFF00) >= (p << 8)){
       C = ((C & ~0xFFFFFF00) | ((C & 0x00FFFF00) - (p << 8)));
       if(A < (1 << 15)){
         if(A < p){
           x = 1 - s;
           if(STATE_CHANGE[contextState[context]] == 1){
             contextMPS[context] = contextMPS[context] == 0 ? 1: 0; //Switches MPS/LPS if necessary
           }
           contextState[context] = STATE_TRANSITIONS_LPS[contextState[context]];
         }else{
           contextState[context] = STATE_TRANSITIONS_MPS[contextState[context]];
         }
         while(A < (1 << 15)){
           if(t == 0){
             fillLSB();
           }
           A <<= 1;
           C <<= 1;
           t--;
         }
       }
     }else{
       if(A < p){
         contextState[context] = STATE_TRANSITIONS_MPS[contextState[context]];
       }else{
         x = 1 - s;
         if(STATE_CHANGE[contextState[context]] == 1){
           contextMPS[context] = contextMPS[context] == 0 ? 1: 0; //Switches MPS/LPS if necessary
         }
         contextState[context] = STATE_TRANSITIONS_LPS[contextState[context]];
       }
       A = p;
       while(A < (1 << 15)){
         if(t == 0){
           fillLSB();
         }
         A <<= 1;
         C <<= 1;
         t--;
       }
     }
     return(x == 1);
   }
 
   /**
    * Encodes a bit using a specified probability.
    *
    * @param bit input
    * @param prob0 through this parameter, the probability of the symbol to be 0 can be set.
    * Let the real probability of the symbol to be 0 be denoted as P. Then, if P >= 0.5, prob0 is
    * computed as prob0 = ((1f - P) * ((4f / 3f) * (float) 0x8000)), otherwise
    * prob0 = - (P * ((4f / 3f) * (float) 0x8000)). Make sure that P = [0.0001f ,0.9999f].
    * This operation is not carried out in the function to alleviate computational costs. See
    * the function {@link #prob0ToMQ} and {@link #MQToProb0}.
    */
   public void encodeBitProb(boolean bit, int prob0){
     int x = bit ? 1 : 0;
     int p;
     int s = 0;
     if(prob0 >= 0){
       p = prob0;
     }else{
       p = -prob0;
       s = 1;
     }
 
     A -= p;
     if(x == s){ //Codes the most probable symbol
       if(A >= (1 << 15)){
         C += p;
       }else{
         if(A < p){
           A = p;
         }else{
           C += p;
         }
         while(A < (1 << 15)){
           A <<= 1;
           C <<= 1;
           t--;
           if(t == 0){
             transferByte();
           }
         }
       }
     }else{ //Codes the least probable symbol
       if(A < p){
         C += p;
       }else{
         A = p;
       }
       while(A < (1 << 15)){
         A <<= 1;
         C <<= 1;
         t--;
         if(t == 0){
           transferByte();
         }
       }
     }
   }
 
   /**
    * Decodes a bit using a specified probability.
    *
    * @param prob0 through this parameter, the probability of the symbol to be 0 can be
    * set. Let the real probability of the symbol to be 0 be denoted as P. Then, if
    * P >= 0.5, prob0 is computed as prob0 = ((1f - P) * ((4f / 3f) * (float) 0x8000)),
    * otherwise prob0 = - (P * ((4f / 3f) * (float) 0x8000)). Make sure that P = [0.0001f ,0.9999f].
    * This operation is not carried out in the function to alleviate computational costs. See the
    * function {@link #prob0ToMQ} and {@link #MQToProb0}.
    *
    * @return output bit
    * @throws Exception when some problem manipulating the stream occurs
    */
   public boolean decodeBitProb(int prob0) throws Exception{
     int p;
     int s = 0;
     if(prob0 >= 0){
       p = prob0;
     }else{
       p = -prob0;
       s = 1;
     }
     int x = s;
 
     A -= p;
     if((C & 0x00FFFF00) >= (p << 8)){
       C = ((C & ~0xFFFFFF00) | ((C & 0x00FFFF00) - (p << 8)));
       if(A < (1 << 15)){
         if(A < p){
           x = 1 - s;
         }
         while(A < (1 << 15)){
           if(t == 0){
             fillLSB();
           }
           A <<= 1;
           C <<= 1;
           t--;
         }
       }
     }else{
       if(A >= p){
         x = 1 - s;
       }
       A = p;
       while(A < (1 << 15)){
         if(t == 0){
           fillLSB();
         }
         A <<= 1;
         C <<= 1;
         t--;
       }
     }
     return(x == 1);
   }
 
   /**
    * Transforms the probability of the symbol 0 (or false) in the range [0:1] into
    * the integer required in the MQ coder to represent that probability.
    *
    * @param prob0 in the range [0:1]
    * @return integer that can be feed to the MQ coder
    */
   public static int prob0ToMQ(float prob0){
     int prob0MQ;
     if(prob0 >= 0.5f){
       prob0 = prob0 > 0.9999f ? 0.9999f: prob0;
       prob0MQ = (int) ((1f - prob0) * ((4f / 3f) * (float) 0x8000));
     }else{
       prob0 = prob0 < 0.0001f ? 0.0001f: prob0;
       prob0MQ = (int) -(prob0 * ((4f / 3f) * (float) 0x8000));
     }
     return(prob0MQ);
   }
 
   /**
    * Transforms the MQ integer to the probability of the symbol 0 (or false) in
    * the range [0:1].
    *
    * @param probMQ integer feed to the MQ coder
    * @return prob0 in the range [0:1]
    */
   public static float MQToProb0(int probMQ){
     float prob0 = (3f * (float) probMQ) / (4f * (float) 0x8000);
     if(probMQ > 0){
       prob0 = 1 - prob0;
     }else{
       prob0 = -prob0;
     }
     return(prob0);
   }
 
   /**
    * Transfers a byte to the stream (for encoding purposes).
    */
   private void transferByte(){
     if(Tr == 0xFF){ //Bit stuff
       stream.putByte((byte) Tr);
       L++;
       Tr = (C >>> 20); //Puts C_msbs to Tr
       C &= (~0xFFF00000); //Puts 0 to C_msbs
       t = 7;
     }else{
       if(C >= 0x08000000){
         //Propagates any carry bit from C into Tr
         Tr += 0x01;
         C &= (~0xF8000000); //Resets the carry bit
       }
       if(L >= 0){
         stream.putByte((byte) Tr);
       }
       L++;
       if(Tr == 0xFF){ //Bit stuff
         //Although it may not be a bit carry
         Tr = (C >>> 20); //Puts C_msbs to Tr
         C &= (~0xFFF00000); //Puts 0 to C_msbs
         t = 7;
       }else{
         Tr = (C >>> 19); //Puts C_partial to Tr
         C &= (~0xFFF80000); //Puts 0 to C_partial
         t = 8;
       }
     }
   }
 
   /**
    * Fills the C register with a byte from the stream or with 0xFF when the end of
    * the stream is reached (for decoding purposes).
    *
    * @throws Exception when some problem manipulating the stream occurs
    */
   private void fillLSB() throws Exception{
     byte BL = 0;
     t = 8;
     if(L < stream.getLength()){
       BL = stream.getByte(L);
     }
     //Reached the end of the stream
     if((L == stream.getLength()) || ((Tr == 0xFF) && (BL > 0x8F))){
       C += 0xFF;
       if(L != stream.getLength()){
         throw new Exception("Read marker 0xFF in the stream.");
       }
     }else{
       if(Tr == 0xFF){
         t = 7;
       }
       Tr = (0x000000FF & (int) BL);
       L++;
       C += (Tr << (8 - t));
     }
   }
 
   /**
    * Changes the current stream. When encoding, before calling this function the stream
    * should be terminated calling the <code>terminate</code> function, and after calling
    * this function the functions <code>restartEncoding</code> and <code>reset</code> must
    * be called. When decoding, after calling this function the functions <code>restartDecoding</code>
    * and <code>reset</code> must be called.
    *
    * @param stream the new ByteStream
    */
   public void changeStream(ByteStream stream){
     if(stream == null){
       stream = new ByteStream();
     }
     this.stream = stream;
   }
 
   /**
    * Resets the state of all contexts.
    */
   public void reset(){
     for(int c = 0; c < numContexts; c++){
       contextState[c] = 0;
       contextMPS[c] = 0;
     }
   }
 
   /**
    * Restarts the internal registers of the coder for encoding.
    */
   public void restartEncoding(){
     A = 0x8000;
     C = 0;
     t = 12;
     Tr = 0;
     L = -1;
   }
 
   /**
    * Restarts the internal registers of the coder for decoding.
    *
    * @throws Exception when some problem manipulating the stream occurs
    */
   public void restartDecoding() throws Exception{
     Tr = 0;
     L  = 0;
     C  = 0;
     fillLSB();
     C <<= t;
     fillLSB();
     C <<= 7;
     t -= 7;
     A = 0x8000;
   }
 
   /**
    * Computes the number of bytes belonging to the currently encoded data needed to flush
    * the internal registers (for encoding purposes). This function is useful to determine
    * potential truncation points of the stream.
    *
    * @return number of bytes
    */
   public int remainingBytes(){
     if(27 - t <= 22){
       return(4);
     }else{
       return(5);
     }
   }
 
   /**
    * Terminates the current stream using the optimal termination (for encoding purposes).
    *
    * @throws Exception when some problem manipulating the stream occurs
    */
   public void terminate() throws Exception{
     terminateOptimal();
   }
 
   /**
    * Gets the number of bytes read or written to the stream associated to the coder.
    *
    * @return the number of bytes
    */
   public int getReadBytes(){
     return(L);
   }
 
   /**
    * Terminates the current stream using the easy termination (for encoding purposes).
    */
   public void terminateEasy(){
     int nBits = 27 - 15 - t;
     C <<= t;
     while(nBits > 0){
       transferByte();
       nBits -= t;
       C <<= t;
     }
     transferByte();
     if(t == 7){
       stream.removeByte();
     }
   }
 
   /**
    * Terminates the current stream using the optimal termination (for encoding purposes).
    *
    * @throws Exception when some problem manipulating the stream occurs
    */
   public void terminateOptimal() throws Exception{
     int NZTr = Tr;
     int NZt = t;
     int NZC = C;
     int NZA = A;
     int NZL = L;
 
     int lengthEmptyTermination = (int) stream.getLength();
     terminateEasy();
     int necessaryBytes = minFlush(NZTr, NZt, NZC, NZA, NZL, lengthEmptyTermination);
     int lengthOptimalTermination = lengthEmptyTermination + necessaryBytes;
 
     if((lengthOptimalTermination >= 1) && ((stream.getByte(lengthOptimalTermination - 1) == 0xFF))){
       lengthOptimalTermination--;
     }
     boolean elimination;
     do{
       elimination = false;
       if((lengthOptimalTermination >= 2) &&
       ((stream.getByte(lengthOptimalTermination - 2) == 0xFF) &&
       (stream.getByte(lengthOptimalTermination - 1) == 0x7F))){
         lengthOptimalTermination -= 2;
         elimination = true;
       }
     }while(elimination);
     stream.removeBytes((int) stream.getLength() - lengthOptimalTermination);
   }
 
   /**
    * Determines the minimum number of bytes needed to terminate the stream while assuring a
    * complete recovering.
    *
    * @param NZTr Tr register for the normalization
    * @param NZt t register for the normalization
    * @param NZC C register for the normalization
    * @param NZA A register for the normalization
    * @param NZL number of flushed bytes
    * @param lengthEmptyTermination length bytes used by the easy termination
    * @return the number of bytes that should be flushed to terminate the ByteStream optimally
    *
    * @throws Exception when some problem manipulating the stream occurs
    */
   private int minFlush(int NZTr, int NZt, int NZC, int NZA, int NZL, int lengthEmptyTermination) throws Exception{
     long Cr = ((long) NZTr << 27) + ((long) NZC << NZt);
     long Ar = (long) NZA << NZt;
     long Rf = 0;
     int s = 8;
     int Sf = 35;
 
     int necessaryBytes = 0;
     int maxNecessaryBytes = 5;
     int cutZone = (int) stream.getLength() - lengthEmptyTermination;
     if(maxNecessaryBytes > cutZone){
       maxNecessaryBytes = cutZone;
     }
     if((lengthEmptyTermination == 0) && (((Cr >> 32) & 0xFF) == 0x00) && (NZL == -1)){
       Cr <<= 8;
       Ar <<= 8;
     }
     while((necessaryBytes < maxNecessaryBytes)
       && ((Rf + ((long) 1 << Sf) - 1 < Cr)
       || (Rf + ((long) 1 << Sf) - 1 >= Cr + Ar))){
       necessaryBytes++;
       if(necessaryBytes <= 4){
         Sf -= s;
         long b = stream.getByte(lengthEmptyTermination + necessaryBytes - 1);
         if(b < 0){
           b += 256;
         }
         Rf += b << Sf;
         if(b == 0xFF){
           s = 7;
         }else{
           s = 8;
         }
       }
     }
     return(necessaryBytes);
   }
 }