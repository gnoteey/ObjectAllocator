/******************************************************************************/
/*!
\file  ObjectAllocator.cpp
\author Ngiam Yee Tong
\par    email: ngiam.y\@digipen.edu
\par    DigiPen login: ngiam.y 
\par    Course: CS280
\par    Assignment-1
\date   27/9/2021
\brief  
    The following functions are implemented in 
    ObjectAllocator.cpp (Assignment 1):
    (1) ObjectAllocator Constructor
    (2) ObjectAllocator Destructor
    (4) Allocate 
    (5) Free 
    (6) DumpMemoryInUse 
    (9) ValidatePages 
*/
/******************************************************************************/

#include "ObjectAllocator.h"
#include <cstring> //memset
#define re_cast reinterpret_cast 

/******************************************************************************/
/*!
    \brief
     The constructor for The ObjectAllocator class
*/
/******************************************************************************/
ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config) 
 :PageList_(NULL), FreeList_(NULL), _Config(config)
{
  //calculate alignment
  if(_Config.Alignment_ > 1) //if there is alignment to do
  {
    unsigned int leftcheck = static_cast<unsigned int>(sizeof(void*) + _Config.HBlockInfo_.size_ 
      + _Config.PadBytes_) % _Config.Alignment_ ;
    unsigned int intercheck = static_cast<unsigned int>(ObjectSize + _Config.HBlockInfo_.size_ 
      + (2*_Config.PadBytes_)) % _Config.Alignment_;

    if(leftcheck)
      _Config.LeftAlignSize_ = _Config.Alignment_ - leftcheck;
    else
      _Config.LeftAlignSize_ = 0;

    if(intercheck)
      _Config.InterAlignSize_ = _Config.Alignment_ - intercheck;
    else
      _Config.InterAlignSize_ = 0;
  }
  // Fill up Stats
  //calculate Midblocks
  midBlockSize = _Config.HBlockInfo_.size_ + (2* _Config.PadBytes_) 
    + ObjectSize + _Config.InterAlignSize_;
  _Stats.PageSize_ = sizeof(void *) + _Config.LeftAlignSize_ 
    + (_Config.ObjectsPerPage_ * midBlockSize) - _Config.InterAlignSize_;
  _Stats.ObjectSize_ = ObjectSize; 

  //If using CPP mm
  if(_Config.UseCPPMemManager_)
  {
    return;
  }
  //allocate new memory
  PageList_ = re_cast<GenericObject*>(Create_NewPage());

}

/******************************************************************************/
/*!
  \brief
   The following destructor function tat destroys the ObjectManager
*/
/******************************************************************************/
ObjectAllocator::~ObjectAllocator() 
{
  if(_Config.UseCPPMemManager_)
  {
    return;
  }

  GenericObject *Page = PageList_;
   
  while(PageList_!=NULL)
  {
    Page = PageList_ -> Next;
    delete PageList_;
    PageList_ = Page;
  }
}

/******************************************************************************/
/*!
  \brief
   The following function is used to allocates a new page
*/
/******************************************************************************/
void *ObjectAllocator::Create_NewPage()
{
  try
  { 
    //Allocate memory for new page
    void *Page = malloc(_Stats.PageSize_);
    void *LeftAlign = re_cast<char*>(Page) + sizeof(void*);
  
    //DEBUGON
    if(_Config.DebugOn_ == true)
    {
      memset(Page, UNALLOCATED_PATTERN, _Stats.PageSize_); //whole page
      memset(LeftAlign, ALIGN_PATTERN, _Config.LeftAlignSize_); //Left align
    }
    
    //update to next ptr
    re_cast<GenericObject*>(Page) -> Next = NULL;
    //update blocks
    void *header = re_cast<char*>(LeftAlign) + _Config.LeftAlignSize_;
    void *leftPAD = re_cast<char*>(header) + _Config.HBlockInfo_.size_;
    void *object = re_cast<char*>(leftPAD) + _Config.PadBytes_;
    void *rightPAD = re_cast<char*>(object) + _Stats.ObjectSize_;
    void *interAlign = re_cast<char*>(rightPAD) + _Config.PadBytes_;

    //FreeList_ = NULL;
    for(size_t i = 0; i < _Config.ObjectsPerPage_; i++)
    {
      //Update header
      memset(header, 0, _Config.HBlockInfo_.size_); 
      //Update Memory Signature
      if(_Config.DebugOn_ == true)
      {
        //Padding
        memset(leftPAD, PAD_PATTERN, _Config.PadBytes_);
        memset(rightPAD, PAD_PATTERN, _Config.PadBytes_);
        //Allignment block
        if((_Config.ObjectsPerPage_ - 1) != i)
        {
          memset(interAlign, ALIGN_PATTERN, _Config.InterAlignSize_);
        }
      }
      //update the free list 
      re_cast<GenericObject*>(object) -> Next = FreeList_;
      FreeList_ = re_cast<GenericObject*>(object);
      //update loop ptr
      header = re_cast<char*>(header) + midBlockSize;
      leftPAD = re_cast<char*>(leftPAD) + midBlockSize;
      object = re_cast<char*>(object) + midBlockSize;
      rightPAD = re_cast<char*>(rightPAD) + midBlockSize;
      interAlign = re_cast<char*>(interAlign) + midBlockSize;
    }
    //Update stats
    _Stats.FreeObjects_ = _Config.ObjectsPerPage_;
    _Stats.PagesInUse_++;
    //return the new allocated page
    return Page;
  }
  catch(std::bad_alloc&)
  {
    throw OAException(OAException::E_NO_MEMORY,"No system memory available");
  }
}

/******************************************************************************/
/*!
  \brief
   The following function acts as new for object allocator class. Take an 
   object from the free list and give it to the client (simulates new). 
   It throws an exception if the object can't be allocated. 

  \param label
   Pointer to a const char

  \return 
   void pointer
*/
/******************************************************************************/
void *ObjectAllocator::Allocate(const char* label) 
{
  if(_Config.UseCPPMemManager_ == true)
  {
    //update stats
    _Stats.Allocations_++;
    _Stats.ObjectsInUse_++;
    if(_Stats.MostObjects_ < _Stats.ObjectsInUse_)
      _Stats.MostObjects_ = _Stats.ObjectsInUse_;
    return malloc(_Stats.ObjectSize_); 
  }
  //Check if No available memory left
  if((FreeList_==nullptr) && (_Config.MaxPages_ != 0) && 
    (_Stats.PagesInUse_ >= _Config.MaxPages_) )
  {
    throw OAException(OAException::E_NO_PAGES,"New pages has been reached");
  }
  //create new page if needed
  if(FreeList_==nullptr)
  {
    GenericObject* newPage = re_cast<GenericObject*>(Create_NewPage());
    newPage -> Next = PageList_;
    PageList_ = newPage;
  }
  void* object = FreeList_;
  FreeList_ = FreeList_ -> Next;

  //update stats
  _Stats.ObjectsInUse_++; 
  _Stats.Allocations_++;
  _Stats.FreeObjects_--;

  if(_Stats.MostObjects_ < _Stats.ObjectsInUse_)
    _Stats.MostObjects_ = _Stats.ObjectsInUse_;
  
  if(_Config.DebugOn_)
  {
    memset(object, ALLOCATED_PATTERN, _Stats.ObjectSize_);
  }

  //update header
  bool* _flag;
  short* _useCount;
  int* _alloc_Num;
  MemBlockInfo** header;

  if(_Config.HBlockInfo_.type_ == OAConfig::hbBasic)
  {
    _flag = re_cast<bool*>(re_cast<char*>(object)
    - (_Config.PadBytes_ + sizeof(bool)));
    *_flag = true;
    _alloc_Num = re_cast<int*>(re_cast<char*>(_flag)-sizeof(int));
    *_alloc_Num = _Stats.Allocations_;
  }

  if(_Config.HBlockInfo_.type_ == OAConfig::hbExtended)
  {
    _flag = re_cast<bool*>(re_cast<char*>(object)
    - (_Config.PadBytes_ + sizeof(bool)));
    *_flag = true;
    _alloc_Num = re_cast<int*>(re_cast<short*>(_flag) - sizeof(short));
    *_alloc_Num = _Stats.Allocations_;
    _useCount = re_cast<short*>(re_cast<char*>(_alloc_Num) - sizeof(short));
    (*_useCount)++; 
  }

  if(_Config.HBlockInfo_.type_ == OAConfig::hbExternal)
  {
    header = re_cast<MemBlockInfo**>(re_cast<char*>(object)
    - (_Config.PadBytes_ + _Config.HBlockInfo_.size_));
    (*header) = re_cast<MemBlockInfo*>(malloc(sizeof(MemBlockInfo)));
    (*header) -> in_use = true;
    (*header) -> alloc_num = _Stats.Allocations_;

    if(label)
    {
      (*header) -> label = re_cast<char*>(malloc(strlen(label) + 1));
      strncpy((*header) -> label, label, strlen(label) + 1);
    }
    else
      (*header)-> label = NULL;
  }
  return object;
} 

/******************************************************************************/
/*!
  \brief
   The following function acts as delete for object allocator class.Returns 
   an object to the free list for the client (simulates delete). Throws an 
   exception if the the object can't be freed. (Invalid object)

  \param Object
  void pointer

  \return
  void pointer
*/
/******************************************************************************/
void ObjectAllocator::Free(void *Object) 
{
  //update stats
  _Stats.Deallocations_++;
  //CPP mm to free object
  if(_Config.UseCPPMemManager_ == true)
  {
    free(Object);
    return;
  }
  //debug check
  if(_Config.DebugOn_ == true)
  {
    GenericObject* temp = FreeList_;
    while(temp!=nullptr)
    {
      if(temp == Object)
      {
        throw OAException(OAException::E_MULTIPLE_FREE,"Object is already Free");
      }
      temp = temp -> Next;
    }
    // Check for object Range
    GenericObject *Page = PageList_;
    void *header = re_cast<char*>(Page) + sizeof(void*) + _Config.LeftAlignSize_;
    void *OBJ = re_cast<char*>(header) + _Config.HBlockInfo_.size_ + _Config.PadBytes_;
    void *EndofPage = re_cast<char*>(Page) + _Stats.PageSize_;
    bool OutofBound = true;

    while(Page)
    {
      size_t Check = re_cast<char*>(Object) - re_cast<char*>(OBJ);
      if((Object >= header) && (Object < EndofPage) && ((Check % midBlockSize) == 0))
      {
        OutofBound = false;
        break;
      }
      Page = Page-> Next;
      header = re_cast<char*>(Page) + sizeof(void*) + _Config.LeftAlignSize_;
      OBJ = re_cast<char*>(header)  + _Config.PadBytes_ + _Config.HBlockInfo_.size_;
      EndofPage = re_cast<char*>(Page) + _Stats.PageSize_;
    }
    if(OutofBound)
    {
      throw OAException(OAException::E_BAD_BOUNDARY,"Object is out of Range");
    }
    //Check for pad corruption
    unsigned char *leftPAD = re_cast<unsigned char*>(Object) - _Config.PadBytes_;
    unsigned char *rightPAD = re_cast<unsigned char*>(Object) + _Stats.ObjectSize_;
    for(size_t i = 1; i < _Config.PadBytes_; i++)
    {
      if(*leftPAD != PAD_PATTERN || *rightPAD != PAD_PATTERN)
      {
        throw OAException(OAException::E_CORRUPTED_BLOCK,"Object is Corrupted");
      }
      leftPAD++;
      rightPAD++;
    }
  }
  //update memory signature
  if(_Config.DebugOn_ == true)
  {
    memset(Object, FREED_PATTERN, _Stats.ObjectSize_);
  }

  //update list
  re_cast<GenericObject*>(Object)->Next = FreeList_;
  FreeList_ = re_cast<GenericObject*>(Object);

  //Update Object Header
  if(_Config.HBlockInfo_.type_ == OAConfig::HBLOCK_TYPE::hbExternal)
  {
    void *header = re_cast<char*>(Object) - 
      (_Config.PadBytes_ + _Config.HBlockInfo_.size_);
    MemBlockInfo** MemBlockInfoPTR = re_cast<MemBlockInfo**>(header);
    if((*MemBlockInfoPTR) -> label)
      free((*MemBlockInfoPTR) -> label);
    free(*MemBlockInfoPTR);
    memset(header, 0, _Config.HBlockInfo_.size_);
  }
  else if(_Config.HBlockInfo_.type_!= OAConfig::HBLOCK_TYPE::hbNone)
  {
    memset(re_cast<char*>(Object) - (_Config.PadBytes_ + sizeof(int) + sizeof(bool)),
     0, (sizeof(int) + sizeof(bool)));
  }
  //update stats
  _Stats.FreeObjects_++;
  _Stats.ObjectsInUse_--;
}

/******************************************************************************/
/*!
  \brief
    The following function calls the callback function for each block in 
    use by the client

  \param fn
   function to call (DUMBCALLBACK)

  \return
   returns the number of block in use by the client
*/
/******************************************************************************/
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
{
  GenericObject *Page = PageList_;
  
  //loop through pages
  while(Page != nullptr)
  {
    void* header = re_cast<char*>(Page) + sizeof(void*) + _Config.LeftAlignSize_;
    void* object = re_cast<char*>(header) + _Config.HBlockInfo_.size_ + _Config.PadBytes_;
    for(size_t i = 0; i<_Config.ObjectsPerPage_; i++)
    {     
      //check if block in use
      GenericObject* tempFreeList = FreeList_;
      bool InUSE = false;
      while(tempFreeList != nullptr)
      {
        if(object == tempFreeList)
        {
          InUSE = true;
          break;
        }
        tempFreeList = tempFreeList -> Next;
      }
      if(InUSE == false)
      {
        fn(object,_Stats.ObjectSize_);
      }
      //Go Next block
      object = re_cast<char*>(object) + midBlockSize;
    }
    Page = Page -> Next;
  }
  return _Stats.ObjectsInUse_;
}

/******************************************************************************/
/*!
  \brief
   The following function call the callback function for each block that is 
   potentially corrupted

  \param fn
   function to call (VALIDATECALLBACK)

  \return
   unsigned
*/
/******************************************************************************/
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
{
  unsigned CorruptedBlks = 0;
  //Only validate pages during debug on
  if(_Config.DebugOn_ == false)
  {
    return CorruptedBlks;
  }

  GenericObject *Page = PageList_;
  //Loop through pages
  while(Page != nullptr)
  {
    void *header = re_cast<char*>(Page) + _Config.LeftAlignSize_ + sizeof(void*) ;
    void *object = re_cast<char*>(header) + _Config.PadBytes_ + _Config.HBlockInfo_.size_ ;
    for(size_t i = 0; i < _Config.ObjectsPerPage_; i++)
    {
      //If padding is corrupted
      unsigned char *leftPAD = re_cast<unsigned char*>(object) - _Config.PadBytes_;
      unsigned char *rightPAD = re_cast<unsigned char*>(object) + _Stats.ObjectSize_;
      for(size_t j = 0; j<_Config.PadBytes_; j++)
      {
        if(*leftPAD != PAD_PATTERN || *rightPAD != PAD_PATTERN)
        {
          fn(object, _Stats.ObjectSize_);
          CorruptedBlks++;
          break;
        }
        leftPAD++;
        rightPAD++;
      }
      // Go Next block
      object = re_cast<char*>(object) + midBlockSize;
    }
    // Next page
    Page = Page -> Next; 
  }
  return CorruptedBlks;
}

/******************************************************************************/
/*!
  \brief
   The following function frees all empty pages(?)

  \return
   unsigned
*/
/******************************************************************************/
unsigned ObjectAllocator::FreeEmptyPages()
{
  unsigned PagesFree = 0;
  //if CPP mm true 
  if(_Config.UseCPPMemManager_|| !PageList_)
  {
    return 0;
  }
  //loop through all pages
  GenericObject *tempPageList = PageList_;
  GenericObject *prevPageList = tempPageList;
  //Loop through page
  while(tempPageList!=nullptr)
  {
    void *header = re_cast<char*>(tempPageList)  + _Config.LeftAlignSize_ + sizeof(void*);
    void *first_Object = re_cast<char*>(header) + _Config.PadBytes_ + _Config.HBlockInfo_.size_;
    void *object = first_Object;
    unsigned NoObjectNotUse = 0;

    for(size_t i = 0; i < _Config.ObjectsPerPage_; i++)
    {
      //check for object in use
      GenericObject *tempFreeList = FreeList_;
      while(tempFreeList != nullptr)
      {
        if(tempFreeList == object)
        {
          NoObjectNotUse++;
          break;
        }
        tempFreeList = tempFreeList -> Next;
      }
      //go next block
      object = re_cast<char*>(object) + midBlockSize;
    }
    //check if all object in page not in use
    if(NoObjectNotUse == _Config.ObjectsPerPage_)
    {
      //Remove object from freelist
     object = first_Object;
      for(size_t i = 0; i < _Config.ObjectsPerPage_; i++)
      {
        //find objeect in freelist
        GenericObject *tmpFreeList = FreeList_;
        GenericObject *prevFreeList = tmpFreeList;
        while(tmpFreeList)
        {
          if(tmpFreeList == object)
          {
            if(tmpFreeList == FreeList_)
            {
              FreeList_ = FreeList_ -> Next;
            }
            else
              prevFreeList -> Next = (*tmpFreeList).Next;
            break;
          }
          prevFreeList = tmpFreeList;
          tmpFreeList = tmpFreeList -> Next;
        }
        //go next object
        object = re_cast<char*>(object) + midBlockSize;
      }
      //free header
      if(_Config.HBlockInfo_.type_ == OAConfig::HBLOCK_TYPE::hbExternal)
      {
        MemBlockInfo** MemPTR = re_cast<MemBlockInfo**>(re_cast<char*>(tempPageList) 
        + _Config.LeftAlignSize_ + sizeof(void*) );
        for(size_t i = 0; i < _Config.ObjectsPerPage_; ++i)
        {
          MemBlockInfo* toFree = *MemPTR;
          MemPTR = re_cast<MemBlockInfo**>(re_cast<char*>(MemPTR) + midBlockSize);
          free(toFree -> label);
          free(toFree);
        }
      }
      //free page list
      GenericObject *PageToFree = tempPageList;
      if(tempPageList == PageList_)
      {
        PageList_ = PageList_ -> Next;
        tempPageList = PageList_;
        prevPageList = tempPageList;
      }
      else 
      {
        prevPageList -> Next = tempPageList -> Next;
        tempPageList = prevPageList -> Next;
      }
      free(PageToFree);
      //Update the stats
      _Stats.FreeObjects_ = _Stats.FreeObjects_ - _Config.ObjectsPerPage_; 
      _Stats.PagesInUse_--;
      PagesFree++;
    }
    else
    {
      //update pointers
      prevPageList = tempPageList;
      tempPageList = tempPageList -> Next;
    }
  }
  //return pages freed
  return PagesFree;
}

/******************************************************************************/
/*!
  \brief
    The following function is used for Testing/Debugging/Statistic methods, 
    set debug state

  \param State
   boolean value
*/
/******************************************************************************/
void ObjectAllocator::SetDebugState(bool State) 
{
  _Config.DebugOn_ = State; // true=enable, false=disable
}

/******************************************************************************/
/*!
  \brief
    The following function is used for Testing/Debugging/Statistic methods 
    and returns a pointer to the internal free list

  \return
   returns a pointer to the internal free list
*/
/******************************************************************************/
const void *ObjectAllocator::GetFreeList() const  
{
  return FreeList_; // returns a pointer to the internal free list
}

/******************************************************************************/
/*!
  \brief
   The following function is used for Testing/Debugging/Statistic methods 
   and returns a pointer to the internal page list

  \return
   returns a pointer to the internal page list
*/
/******************************************************************************/
const void *ObjectAllocator::GetPageList() const 
{
  return PageList_;  // returns a pointer to the internal page list
}

/******************************************************************************/
/*!
  \brief
   The following function is used for Testing/Debugging/Statistic methods 
   and returns the configuration parameters

  \return
   the configuration parameters
*/
/******************************************************************************/
OAConfig ObjectAllocator::GetConfig() const
{
  return _Config;  // returns the configuration parameters
}

/******************************************************************************/
/*!
  \brief
   The following function is used for Testing/Debugging/Statistic methods 
   and returns statistics for the allocator

  \return
   the statistics for the allocator
*/
/******************************************************************************/
OAStats ObjectAllocator::GetStats() const 
{
  return _Stats;  // returns the statistics for the allocator
}